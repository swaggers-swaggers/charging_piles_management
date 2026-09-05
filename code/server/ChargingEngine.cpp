#include "ChargingEngine.h"

#include "DatabaseManager.h"
#include "network/ClientHandler.h"
#include "dao/OrderDao.h"
#include "dao/PileDao.h"
#include "dao/PriceRuleDao.h"
#include "dao/ReservationDao.h"
#include "dao/UserDao.h"
#include "protocol.h"

#include <QDateTime>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>
#include <QTimer>
#include <QtMath>

using namespace Protocol;

ChargingEngine &ChargingEngine::instance()
{
    static ChargingEngine s;
    return s;
}

ChargingEngine::ChargingEngine(QObject *parent)
    : QObject(parent)
{
}

void ChargingEngine::start()
{
    if (m_started)
        return;
    m_started = true;
    qRegisterMetaType<QJsonObject>("QJsonObject");
    recoverOnStart();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ChargingEngine::onTick);
    m_timer->start(ChargeConfig::kTickMs);
}

// ---------------------------------------------------------------------------
// 在线连接注册表
// ---------------------------------------------------------------------------
void ChargingEngine::registerClient(int userId, ClientHandler *handler)
{
    if (userId <= 0 || !handler)
        return;
    QMutexLocker lock(&m_onlineMutex);
    m_online.insert(userId, handler);
}

void ChargingEngine::unregisterClient(ClientHandler *handler)
{
    if (!handler)
        return;
    QMutexLocker lock(&m_onlineMutex);
    for (auto it = m_online.begin(); it != m_online.end(); ++it) {
        if (it.value() == handler) {
            m_online.erase(it);
            break;
        }
    }
}

void ChargingEngine::pushToUser(int userId, const QJsonObject &msg)
{
    ClientHandler *handler = nullptr;
    {
        QMutexLocker lock(&m_onlineMutex);
        handler = m_online.value(userId, nullptr);
    }
    if (handler) {
        QMetaObject::invokeMethod(handler, "pushToClient",
                                  Qt::QueuedConnection,
                                  Q_ARG(QJsonObject, msg));
    }
}

// ---------------------------------------------------------------------------
// 冻结额计算
// ---------------------------------------------------------------------------
double ChargingEngine::calcFreeze(int targetType, double targetValue,
                                  double power, double unitPrice,
                                  double balance, int *errorCode)
{
    if (errorCode)
        *errorCode = ErrNone;
    double freeze = 0.0;
    switch (targetType) {
    case TargetNone:
        if (balance < ChargeConfig::kMinFreeze) {
            if (errorCode) *errorCode = ErrFreezeNotEnough;
            return -1.0;
        }
        freeze = qMin(ChargeConfig::kDefaultFreeze, balance);
        break;
    case TargetEnergy:
        if (targetValue <= 0 || targetValue > 500) {
            if (errorCode) *errorCode = ErrTargetInvalid;
            return -1.0;
        }
        freeze = targetValue * unitPrice * 1.2;
        break;
    case TargetAmount:
        if (targetValue <= 0 || targetValue > 100000) {
            if (errorCode) *errorCode = ErrTargetInvalid;
            return -1.0;
        }
        freeze = targetValue;
        break;
    case TargetMinutes:
        if (targetValue <= 0 || targetValue > 24 * 60) {
            if (errorCode) *errorCode = ErrTargetInvalid;
            return -1.0;
        }
        freeze = power * targetValue / 60.0 * unitPrice * 1.2;
        break;
    default:
        if (errorCode) *errorCode = ErrTargetInvalid;
        return -1.0;
    }
    if (freeze > balance + 1e-6) {
        if (errorCode) *errorCode = ErrFreezeNotEnough;
        return -1.0;
    }
    return qRound(freeze * 100.0) / 100.0;
}

// ---------------------------------------------------------------------------
// 开启充电: 校验 → 事务(原子抢桩 + 冻结 + 建单)
// ---------------------------------------------------------------------------
ChargingEngine::StartResult ChargingEngine::startCharging(int userId, int pileId,
                                                          int targetType, double targetValue,
                                                          const QString &connName)
{
    StartResult r;
    QSqlDatabase db = connName.isEmpty() ? DatabaseManager::instance().database()
                                         : QSqlDatabase::database(connName);

    UserInfo user;
    if (!UserDao::getById(userId, &user, nullptr, connName)) {
        r.errorCode = ErrNotFound; r.error = "用户不存在"; return r;
    }
    if (user.status == UserFrozen) {
        r.errorCode = ErrUserFrozen; r.error = "账号已被冻结, 无法充电"; return r;
    }

    PileInfo pile = PileDao::getById(pileId, nullptr, connName);
    if (pile.id == 0) {
        r.errorCode = ErrNotFound; r.error = "充电桩不存在"; return r;
    }
    if (pile.status == PileFault) {
        r.errorCode = ErrPileBusy; r.error = "该充电桩故障中, 请选择其他桩"; return r;
    }

    bool hasOrder = false;
    OrderDao::getUnfinishedByUser(userId, &hasOrder, nullptr, connName);
    if (hasOrder) {
        r.errorCode = ErrOrderExists; r.error = "您已有正在充电的订单, 请先结束"; return r;
    }

    // 计费单价: 优先分时费率, 无配置回退站点基准价
    double unitPrice = 0.0, serviceFee = 0.0;
    if (!PriceRuleDao::currentPrice(pile.stationId, &unitPrice, &serviceFee,
                                    QTime(), nullptr, connName) || unitPrice <= 0) {
        QSqlQuery sp(db);
        sp.prepare("SELECT price FROM station WHERE id=?");
        sp.addBindValue(pile.stationId);
        if (sp.exec() && sp.next())
            unitPrice = sp.value(0).toDouble();
    }
    if (unitPrice <= 0)
        unitPrice = 1.0;

    int ec = ErrNone;
    const double freeze = calcFreeze(targetType, targetValue, pile.power,
                                     unitPrice, user.balance, &ec);
    if (ec != ErrNone) {
        r.errorCode = ec;
        r.error = (ec == ErrFreezeNotEnough)
                      ? QString("余额不足以预授权冻结(当前余额 %1 元)").arg(user.balance, 0, 'f', 2)
                      : "充电目标参数不合法";
        return r;
    }

    if (!db.transaction()) {
        r.errorCode = ErrDbError; r.error = db.lastError().text(); return r;
    }
    QString err;
    // 1) 原子抢桩: 仅空闲能抢到
    if (!PileDao::acquire(pileId, &err, connName)) {
        db.rollback();
        r.errorCode = ErrPileBusy; r.error = "手慢了, 该桩刚被占用"; return r;
    }
    // 2) 冻结预授权额
    if (!UserDao::adjustBalance(userId, -freeze, &err, connName)) {
        db.rollback(); r.errorCode = ErrDbError; r.error = err; return r;
    }
    // 3) 建单
    const int orderId = OrderDao::create(userId, pileId, pile.stationId,
                                         qRound(unitPrice * 1000) / 1000.0, freeze,
                                         targetType, targetValue, &err, connName);
    if (orderId <= 0) {
        db.rollback(); r.errorCode = ErrDbError; r.error = err; return r;
    }
    // 4) 若该用户今日预约了此桩, 预约置为已履约; 待确认的现场排队同样置履约
    ReservationDao::fulfillTodayAppoint(userId, pileId, nullptr, connName);
    QSqlQuery fulfillQueue(db);
    fulfillQueue.prepare("UPDATE charge_reservation SET status=4"
                         " WHERE user_id=? AND pile_id=? AND type=0 AND status=1");
    fulfillQueue.addBindValue(userId);
    fulfillQueue.addBindValue(pileId);
    fulfillQueue.exec();

    if (!db.commit()) {
        db.rollback(); r.errorCode = ErrDbError; r.error = db.lastError().text(); return r;
    }

    r.ok = true;
    r.freezeAmount = freeze;
    r.unitPrice = unitPrice;
    r.order = OrderDao::getById(orderId, nullptr, connName);
    UserInfo after;
    if (UserDao::getById(userId, &after, nullptr, connName))
        r.balanceAfter = after.balance;
    return r;
}

// ---------------------------------------------------------------------------
// 统一结算
// ---------------------------------------------------------------------------
ChargingEngine::SettleResult ChargingEngine::settleOrder(int orderId, int finishType,
                                                         const QString &reason,
                                                         const QString &connName)
{
    SettleResult r;
    QSqlDatabase db = connName.isEmpty() ? DatabaseManager::instance().database()
                                         : QSqlDatabase::database(connName);

    if (!db.transaction()) { r.error = db.lastError().text(); return r; }

    int userId = -1, pileId = -1, simMin = 0, curStatus = -1;
    double energy = 0, amount = 0, freeze = 0;
    QSqlQuery q(db);
    q.prepare("SELECT user_id, pile_id, energy, amount, freeze_amount, sim_minutes, status"
              " FROM charge_order WHERE id=?");
    q.addBindValue(orderId);
    if (!q.exec() || !q.next()) {
        db.rollback(); r.error = "订单不存在"; return r;
    }
    userId = q.value(0).toInt();
    pileId = q.value(1).toInt();
    energy = q.value(2).toDouble();
    amount = q.value(3).toDouble();
    freeze = q.value(4).toDouble();
    simMin = q.value(5).toInt();
    curStatus = q.value(6).toInt();
    if (curStatus != OrderCharging) {
        db.rollback(); r.error = "订单不在充电中, 无法结算"; return r;
    }

    // 1) 解冻冻结额并实扣: balance += freeze - amount
    QSqlQuery bal(db);
    bal.prepare("UPDATE user SET balance=balance+?-? WHERE id=?");
    bal.addBindValue(freeze);
    bal.addBindValue(amount);
    bal.addBindValue(userId);
    if (!bal.exec()) { db.rollback(); r.error = bal.lastError().text(); return r; }

    // 2) 落单: 故障 → status=4 异常中断, 其余 → status=1 已完成
    const int newStatus = (finishType == FinishByFault) ? OrderAbnormal : OrderFinished;
    QSqlQuery fin(db);
    fin.prepare("UPDATE charge_order SET end_time=datetime('now','localtime'),"
                " energy=?, amount=?, sim_minutes=?, status=?, finish_type=?, cancel_reason=?"
                " WHERE id=?");
    fin.addBindValue(energy);
    fin.addBindValue(amount);
    fin.addBindValue(simMin);
    fin.addBindValue(newStatus);
    fin.addBindValue(finishType);
    fin.addBindValue(reason);
    fin.addBindValue(orderId);
    if (!fin.exec()) { db.rollback(); r.error = fin.lastError().text(); return r; }

    // 3) 累计使用时长并释放桩; 故障桩(status=2)保持故障, 不重新变为空闲
    QSqlQuery rel(db);
    rel.prepare("UPDATE pile SET status=CASE WHEN status=2 THEN 2 ELSE 0 END,"
                " total_count=total_count+1,"
                " total_duration=total_duration+? WHERE id=?");
    rel.addBindValue(simMin);
    rel.addBindValue(pileId);
    if (!rel.exec()) { db.rollback(); r.error = rel.lastError().text(); return r; }

    if (!db.commit()) { db.rollback(); r.error = db.lastError().text(); return r; }

    r.ok = true;
    r.order = OrderDao::getById(orderId, nullptr, connName);
    UserInfo u;
    if (UserDao::getById(userId, &u, nullptr, connName))
        r.balanceAfter = u.balance;

    // 4) 释放后尝试分配排队队首
    assignQueueHead(pileId, connName);
    return r;
}

ChargingEngine::SettleResult ChargingEngine::forceFinish(int orderId, const QString &reason)
{
    SettleResult r = settleOrder(orderId, FinishByAdmin,
                                 reason.isEmpty() ? QStringLiteral("管理员强制结束") : reason);
    if (r.ok)
        notifyOrderEnded(r.order, FinishByAdmin, reason);
    return r;
}

bool ChargingEngine::refundOrder(int orderId, double amount, QString *err,
                                 const QString &connName)
{
    if (amount <= 0) {
        if (err) *err = "退款金额必须大于 0";
        return false;
    }
    QSqlDatabase db = connName.isEmpty() ? DatabaseManager::instance().database()
                                         : QSqlDatabase::database(connName);
    const OrderInfo o = OrderDao::getById(orderId, nullptr, connName);
    if (o.id == 0) { if (err) *err = "订单不存在"; return false; }

    if (!db.transaction()) { if (err) *err = db.lastError().text(); return false; }
    QSqlQuery rb(db);
    rb.prepare("UPDATE user SET balance=balance+? WHERE id=?");
    rb.addBindValue(amount);
    rb.addBindValue(o.userId);
    if (!rb.exec()) { db.rollback(); if (err) *err = rb.lastError().text(); return false; }
    QSqlQuery ro(db);
    ro.prepare("UPDATE charge_order SET refund_amount=refund_amount+? WHERE id=?");
    ro.addBindValue(amount);
    ro.addBindValue(orderId);
    if (!ro.exec()) { db.rollback(); if (err) *err = ro.lastError().text(); return false; }
    if (!db.commit()) { db.rollback(); if (err) *err = db.lastError().text(); return false; }
    return true;
}

// ---------------------------------------------------------------------------
// 排队分配
// ---------------------------------------------------------------------------
void ChargingEngine::assignQueueHead(int pileId, const QString &connName)
{
    PileInfo pile = PileDao::getById(pileId, nullptr, connName);
    if (pile.id == 0 || pile.status != PileIdle)
        return;   // 桩不空闲(被新用户直接抢走等情况), 不分配

    ReservationInfo head = ReservationDao::nextPending(pileId, nullptr, connName);
    if (head.id == 0)
        return;
    if (!ReservationDao::markAssigned(head.id, ChargeConfig::kQueueConfirmSec,
                                      nullptr, connName))
        return;

    QJsonObject ev;
    ev.insert("type", PushOrderEvent);
    ev.insert("event", 1);
    ev.insert("reservationId", head.id);
    ev.insert("pileId", pileId);
    ev.insert("pileCode", pile.code);
    ev.insert("message", QStringLiteral("排队轮到您了, 请在 %1 秒内确认开始充电")
                             .arg(ChargeConfig::kQueueConfirmSec));
    pushToUser(head.userId, ev);
}

// ---------------------------------------------------------------------------
// 启动恢复
// ---------------------------------------------------------------------------
void ChargingEngine::recoverOnStart()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    // 孤儿桩: 状态在用但没有任何充电中订单引用 → 释放为空闲
    QSqlQuery q(db);
    q.exec("UPDATE pile SET status=0 WHERE status=1 AND id NOT IN"
           " (SELECT pile_id FROM charge_order WHERE status=0)");
    // status=0 的充电订单由 onTick 自动继续推进(进度全部在库中, 无需内存状态)
}

// ---------------------------------------------------------------------------
// 心跳: 推进充电 + 排队/预约扫描
// ---------------------------------------------------------------------------
void ChargingEngine::onTick()
{
    sweepActiveOrders();
    sweepReservations();
}

void ChargingEngine::sweepActiveOrders()
{
    const QList<OrderInfo> actives = OrderDao::listActive();
    for (const OrderInfo &o : actives) {
        const OrderDao::OrderContext ctx = OrderDao::getContext(o.id);
        if (!ctx.exists)
            continue;

        // 桩故障 → 异常中断
        if (ctx.pileStatus == PileFault) {
            SettleResult sr = settleOrder(o.id, FinishByFault,
                                          QStringLiteral("充电过程中电桩故障, 充电中断"));
            if (sr.ok) {
                QJsonObject ev;
                ev.insert("type", PushOrderEvent);
                ev.insert("event", 3);
                ev.insert("orderId", o.id);
                ev.insert("order", sr.order.toJson());
                ev.insert("message", QStringLiteral("充电桩故障, 充电已中断, 已消费金额可联系管理员退款"));
                pushToUser(ctx.userId, ev);
            }
            continue;
        }

        int newMin = ctx.simMinutes + ChargeConfig::kMinutesPerTick;
        double newEnergy = ctx.energy + ctx.power * ChargeConfig::kMinutesPerTick / 60.0;
        double newAmount = newEnergy * ctx.priceSnapshot;

        int finish = -1;
        QString reason;
        // 余额耗尽: 可用 = 当前余额 + 冻结 - 预计消费
        const double remain = ctx.userBalance + ctx.freezeAmount - newAmount;
        if (remain <= 0.01) {
            newAmount = ctx.freezeAmount + ctx.userBalance;
            if (newAmount < 0) newAmount = 0;
            newEnergy = ctx.priceSnapshot > 0 ? newAmount / ctx.priceSnapshot : newEnergy;
            finish = FinishByBalance;
            reason = QStringLiteral("余额已用完, 自动结束充电");
        } else if (ctx.targetType == TargetEnergy && ctx.targetValue > 0
                   && newEnergy >= ctx.targetValue) {
            newEnergy = ctx.targetValue;
            newAmount = newEnergy * ctx.priceSnapshot;
            finish = FinishByTarget;
            reason = QStringLiteral("已达到设定电量目标 %1 度, 自动结束").arg(ctx.targetValue);
        } else if (ctx.targetType == TargetAmount && ctx.targetValue > 0
                   && newAmount >= ctx.targetValue) {
            newAmount = ctx.targetValue;
            newEnergy = ctx.priceSnapshot > 0 ? newAmount / ctx.priceSnapshot : newEnergy;
            finish = FinishByTarget;
            reason = QStringLiteral("已达到设定金额目标 %1 元, 自动结束").arg(ctx.targetValue);
        } else if (ctx.targetType == TargetMinutes && ctx.targetValue > 0
                   && newMin >= int(ctx.targetValue)) {
            finish = FinishByTarget;
            reason = QStringLiteral("已达到设定时长目标 %1 分钟, 自动结束")
                         .arg(int(ctx.targetValue));
        }

        newEnergy = qRound(newEnergy * 1000.0) / 1000.0;
        newAmount = qRound(newAmount * 100.0) / 100.0;

        if (finish >= 0) {
            OrderDao::updateProgress(o.id, newEnergy, newAmount, newMin);
            SettleResult sr = settleOrder(o.id, finish, reason);
            if (sr.ok)
                notifyOrderEnded(sr.order, finish, reason);
            continue;
        }

        OrderDao::updateProgress(o.id, newEnergy, newAmount, newMin);

        double progress = 0.0;
        if (ctx.targetType == TargetEnergy && ctx.targetValue > 0)
            progress = newEnergy / ctx.targetValue;
        else if (ctx.targetType == TargetAmount && ctx.targetValue > 0)
            progress = newAmount / ctx.targetValue;
        else if (ctx.targetType == TargetMinutes && ctx.targetValue > 0)
            progress = newMin / ctx.targetValue;

        QJsonObject push;
        push.insert("type", PushOrderProgress);
        push.insert("orderId", o.id);
        push.insert("energy", newEnergy);
        push.insert("amount", newAmount);
        push.insert("minutes", newMin);
        push.insert("power", ctx.power);
        push.insert("targetType", ctx.targetType);
        push.insert("targetValue", ctx.targetValue);
        push.insert("targetProgress", qBound(0.0, progress, 1.0));
        pushToUser(ctx.userId, push);
    }
}

void ChargingEngine::sweepReservations()
{
    // 1) 已分配但超时未确认 → 过期并顺延下一位
    const QList<ReservationInfo> expired = ReservationDao::listAssignedExpired();
    for (const ReservationInfo &r : expired) {
        ReservationDao::setStatus(r.id, ReservationExpired);
        QJsonObject ev;
        ev.insert("type", PushOrderEvent);
        ev.insert("event", 7);
        ev.insert("reservationId", r.id);
        ev.insert("message", QStringLiteral("排队确认超时, 已为您取消本次排队"));
        pushToUser(r.userId, ev);
        assignQueueHead(r.pileId);
    }

    // 2) 预约开始前 10 分钟提醒
    const QList<ReservationInfo> remind = ReservationDao::listAppointRemindDue();
    for (const ReservationInfo &r : remind) {
        ReservationDao::markRemindSent(r.id);
        QJsonObject ev;
        ev.insert("type", PushOrderEvent);
        ev.insert("event", 6);
        ev.insert("reservationId", r.id);
        ev.insert("pileCode", r.pileCode);
        ev.insert("reserveStart", r.reserveStart);
        ev.insert("message", QStringLiteral("您预约的 %1 充电桩将于 %2 开始, 请按时到场")
                                 .arg(r.pileCode, r.reserveStart));
        pushToUser(r.userId, ev);
    }

    // 3) 超过结束宽限期仍未履约 → 预约过期
    const QList<ReservationInfo> apExpired = ReservationDao::listAppointExpired();
    for (const ReservationInfo &r : apExpired) {
        ReservationDao::setStatus(r.id, ReservationExpired);
        QJsonObject ev;
        ev.insert("type", PushOrderEvent);
        ev.insert("event", 7);
        ev.insert("reservationId", r.id);
        ev.insert("message", QStringLiteral("预约时段已过且未到桩充电, 预约已失效"));
        pushToUser(r.userId, ev);
    }
}

void ChargingEngine::notifyOrderEnded(const OrderInfo &order, int finishType,
                                      const QString &reason)
{
    QJsonObject ev;
    ev.insert("type", PushOrderEvent);
    ev.insert("event", finishType == FinishByFault ? 3 : 2);
    ev.insert("orderId", order.id);
    ev.insert("order", order.toJson());
    ev.insert("finishType", finishType);
    ev.insert("message", reason);
    pushToUser(order.userId, ev);
}
