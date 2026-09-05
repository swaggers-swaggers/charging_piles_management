#include "ClientHandler.h"

#include "ChargingEngine.h"
#include "DatabaseManager.h"
#include "Predictor.h"
#include "dao/OrderDao.h"
#include "dao/PileDao.h"
#include "dao/PriceRuleDao.h"
#include "dao/ReservationDao.h"
#include "dao/StationDao.h"
#include "dao/UserDao.h"
#include "protocol.h"
#include "types.h"
#include "GeoUtil.h"

#include <algorithm>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_descriptor(socketDescriptor)
{
}

ClientHandler::~ClientHandler()
{
    ChargingEngine::instance().unregisterClient(this);
    if (!m_dbConnName.isEmpty()) {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
        if (db.isOpen())
            db.close();
        db = QSqlDatabase();   // 释放引用后再移除, 避免Qt告警
        QSqlDatabase::removeDatabase(m_dbConnName);
    }
}

void ClientHandler::start()
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(m_descriptor)) {
        qDebug() << "[ClientHandler] 套接字初始化失败";
        emit finished();
        return;
    }

    // 本线程私有的数据库连接
    m_dbConnName = QString("client-%1")
                       .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnName);
    db.setDatabaseName(DatabaseManager::instance().databasePath());
    if (db.open()) {
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA busy_timeout=3000");
    } else {
        qDebug() << "[ClientHandler] 数据库连接失败:" << db.lastError().text();
        // 连接保留, 具体业务请求会以数据库错误应答
    }

    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ClientHandler::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_socket->error() != QAbstractSocket::RemoteHostClosedError)
            qDebug() << "[ClientHandler] 套接字错误:" << m_socket->errorString();
    });

    qDebug() << "[ClientHandler] 客户端接入:"
             << m_socket->peerAddress().toString() << m_socket->peerPort();
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    if (m_buffer.size() > 1024 * 1024) {
        qDebug() << "[ClientHandler] 缓冲区超限, 断开连接";
        m_socket->disconnectFromHost();
        return;
    }

    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        processLine(line);
    }
}

void ClientHandler::onDisconnected()
{
    qDebug() << "[ClientHandler] 客户端断开 userId=" << m_userId;
    // v2: 断线不再停止充电, 引擎在主线程继续推进
    ChargingEngine::instance().unregisterClient(this);
    emit finished();
}

void ClientHandler::processLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty())
        return;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        sendError(0, "请求格式错误");
        return;
    }
    handleRequest(doc.object());
}

void ClientHandler::handleRequest(const QJsonObject &request)
{
    const int type = request.value("type").toInt();
    QJsonObject reply;

    switch (type) {
    case Protocol::ReqHeartbeat:
        reply = Protocol::makeReply(type, true);
        break;
    case Protocol::ReqUserLogin:        reply = processUserLogin(request);        break;
    case Protocol::ReqGetUserInfo:      reply = processGetUserInfo(request);      break;
    case Protocol::ReqUpdateProfile:    reply = processUpdateProfile(request);    break;
    case Protocol::ReqRecharge:         reply = processRecharge(request);         break;
    case Protocol::ReqStationList:      reply = processStationList(request);      break;
    case Protocol::ReqStationPiles:     reply = processStationPiles(request);     break;
    case Protocol::ReqUnfinishedOrder:  reply = processUnfinishedOrder(request);  break;
    case Protocol::ReqStartCharge:      reply = startChargeInternal(type, request); break;
    case Protocol::ReqStopCharge:       reply = processStopCharge(request);       break;
    case Protocol::ReqStartChargeExt:   reply = startChargeInternal(type, request); break;
    case Protocol::ReqReservePile:      reply = processReservePile(request);      break;
    case Protocol::ReqAppointPile:      reply = processAppointPile(request);      break;
    case Protocol::ReqAppointSlots:     reply = processAppointSlots(request);     break;
    case Protocol::ReqMyReservations:   reply = processMyReservations(request);   break;
    case Protocol::ReqOrderHistory:     reply = processOrderHistory(request);     break;
    case Protocol::ReqOrderDetail:      reply = processOrderDetail(request);      break;
    case Protocol::ReqStationFee:       reply = processStationFee(request);       break;
    default:
        sendError(type, "暂不支持的消息类型");
        return;
    }

    sendJson(reply);
}

// ---------- 登录 ----------

QJsonObject ClientHandler::processUserLogin(const QJsonObject &req)
{
    const QString phone = req.value("phone").toString().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch())
        return Protocol::makeReply(Protocol::ReqUserLogin, false, "手机号格式不正确!");

    UserInfo info;
    bool isNewUser = false;
    QString errMsg;
    if (!DatabaseManager::instance().loginOrRegisterUser(phone, &info, &isNewUser,
                                                         &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUserLogin, false, errMsg);

    m_userId = info.id;
    // 注册到引擎, 充电推进/排队/预约事件可推送到本连接
    ChargingEngine::instance().registerClient(m_userId, this);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqUserLogin, true);
    reply.insert("userId", info.id);
    reply.insert("phone", info.phone);
    reply.insert("nickname", info.nickname);
    reply.insert("balance", info.balance);
    reply.insert("isNew", isNewUser);
    qDebug() << "[ClientHandler] 用户登录:" << info.phone
             << (isNewUser ? "(新注册)" : "") << "nickname=" << info.nickname;
    return reply;
}

// ---------- 用户信息 ----------

QJsonObject ClientHandler::processGetUserInfo(const QJsonObject &req)
{
    Q_UNUSED(req)
    const int userId = m_userId;
    UserInfo u;
    QString errMsg;
    if (!UserDao::getById(userId, &u, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqGetUserInfo, false, errMsg);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqGetUserInfo, true);
    reply.insert("nickname", u.nickname);
    reply.insert("balance", u.balance);
    reply.insert("avatar", u.avatar);
    reply.insert("status", u.status);
    return reply;
}

QJsonObject ClientHandler::processUpdateProfile(const QJsonObject &req)
{
    const int userId = m_userId;
    QString errMsg;
    if (!UserDao::updateProfile(userId,
                                req.value("nickname").toString(),
                                req.value("avatar").toString(),
                                &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUpdateProfile, false, errMsg);

    // 回显请求 type(ReqUpdateProfile), 并返回更新后的用户信息(与 ReqGetUserInfo 字段一致)
    UserInfo u;
    if (!UserDao::getById(userId, &u, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUpdateProfile, false, errMsg);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqUpdateProfile, true);
    reply.insert("nickname", u.nickname);
    reply.insert("balance", u.balance);
    reply.insert("avatar", u.avatar);
    reply.insert("status", u.status);
    return reply;
}

QJsonObject ClientHandler::processRecharge(const QJsonObject &req)
{
    const int userId = m_userId;
    const double amount = req.value("amount").toDouble();

    if (amount <= 0 || amount > 10000)
        return Protocol::makeReply(Protocol::ReqRecharge, false, "充值金额需在 0.01 ~ 10000 元之间!");

    double newBalance = 0;
    QString errMsg;
    if (!UserDao::recharge(userId, amount, &newBalance, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqRecharge, false, errMsg);

    // 充值流水
    QSqlQuery log(QSqlDatabase::database(m_dbConnName));
    log.prepare("INSERT INTO recharge_log(user_id, amount, balance_after) VALUES(?,?,?)");
    log.addBindValue(userId);
    log.addBindValue(amount);
    log.addBindValue(newBalance);
    log.exec();

    qDebug() << "[ClientHandler] 用户" << userId << "充值" << amount << "元, 余额" << newBalance;

    // 充值成功推送消息(消息系统)
    QJsonObject recEv;
    recEv.insert("type", Protocol::PushOrderEvent);
    recEv.insert("event", 9);   // 9=充值成功
    recEv.insert("balance", newBalance);
    recEv.insert("message", QStringLiteral("充值 %1 元成功, 当前余额 %2 元")
                                 .arg(amount, 0, 'f', 2).arg(newBalance, 0, 'f', 2));
    ChargingEngine::instance().pushToUser(userId, recEv);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqRecharge, true);
    reply.insert("balance", newBalance);
    return reply;
}

// ---------- 充电站查询 ----------

QJsonObject ClientHandler::processStationList(const QJsonObject &req)
{
    const double lon = req.value("lon").toDouble(123.45);
    const double lat = req.value("lat").toDouble(41.70);

    QList<StationInfo> stations = StationDao::list(m_dbConnName);
    stations.erase(std::remove_if(stations.begin(), stations.end(),
                                  [](const StationInfo &s) { return s.totalPiles <= 0; }),
                   stations.end());
    for (StationInfo &s : stations) {
        s.distance = GeoUtil::haversineKm(lat, lon, s.latitude, s.longitude);
        s.predictIdle = Predictor::predictIdleRate(s.id, s.totalPiles, s.idlePiles, 1,
                                                    m_dbConnName);
    }
    std::sort(stations.begin(), stations.end(),
              [](const StationInfo &a, const StationInfo &b) { return a.distance < b.distance; });

    QJsonArray arr;
    for (const StationInfo &s : stations)
        arr.append(s.toJson());

    QJsonObject reply = Protocol::makeReply(Protocol::ReqStationList, true);
    reply.insert("stations", arr);
    return reply;
}

QJsonObject ClientHandler::processStationPiles(const QJsonObject &req)
{
    const int stationId = req.value("stationId").toInt();
    if (stationId <= 0)
        return Protocol::makeReply(Protocol::ReqStationPiles, false, "参数错误: 缺少stationId");

    const QList<PileInfo> piles = PileDao::listByStation(stationId, m_dbConnName);
    QJsonArray arr;
    for (const PileInfo &p : piles)
        arr.append(p.toJson());

    QJsonObject reply = Protocol::makeReply(Protocol::ReqStationPiles, true);
    reply.insert("piles", arr);
    return reply;
}

// ---------- 充电与订单 ----------

QJsonObject ClientHandler::processUnfinishedOrder(const QJsonObject &req)
{
    Q_UNUSED(req)
    const int userId = m_userId;
    bool has = false;
    QString errMsg;
    const OrderInfo order = OrderDao::getUnfinishedByUser(userId, &has, &errMsg, m_dbConnName);
    if (!errMsg.isEmpty() && !has)
        return Protocol::makeReply(Protocol::ReqUnfinishedOrder, false, errMsg);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqUnfinishedOrder, true);
    reply.insert("hasOrder", has);
    if (has)
        reply.insert("order", order.toJson());
    return reply;
}

QJsonObject ClientHandler::startChargeInternal(int replyType, const QJsonObject &req)
{
    const int userId = m_userId;
    const int pileId = req.value("pileId").toInt();
    if (pileId <= 0)
        return Protocol::makeReply(replyType, false, "参数错误: 缺少pileId");

    const int targetType = req.value("targetType").toInt(TargetNone);
    const double targetValue = req.value("targetValue").toDouble(0.0);

    const ChargingEngine::StartResult r =
        ChargingEngine::instance().startCharging(userId, pileId, targetType, targetValue,
                                                 m_dbConnName);
    if (!r.ok) {
        QJsonObject fail = Protocol::makeReply(replyType, false, r.error);
        fail.insert("errorCode", r.errorCode);
        return fail;
    }

    qDebug() << "[ClientHandler] 用户" << userId << "开始充电, 订单" << r.order.id
             << "冻结" << r.freezeAmount << "元";

    QJsonObject reply = Protocol::makeReply(replyType, true);
    reply.insert("order", r.order.toJson());
    reply.insert("freezeAmount", r.freezeAmount);
    reply.insert("price", r.unitPrice);
    reply.insert("balance", r.balanceAfter);
    return reply;
}

QJsonObject ClientHandler::processStopCharge(const QJsonObject &req)
{
    const int userId = m_userId;
    const int orderId = req.value("orderId").toInt();
    if (orderId <= 0)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "参数错误: 缺少orderId");

    const OrderInfo before = OrderDao::getById(orderId, nullptr, m_dbConnName);
    if (before.id == 0)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "订单不存在");
    if (before.userId != userId)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "无权操作该订单");
    if (before.status != OrderCharging)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "订单已结束, 无需重复结算");

    const ChargingEngine::SettleResult sr =
        ChargingEngine::instance().settleOrder(orderId, FinishByUser,
                                               QStringLiteral("用户主动结束"), m_dbConnName);
    if (!sr.ok)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, sr.error);

    qDebug() << "[ClientHandler] 订单" << orderId << "结算: 电量" << sr.order.energy
             << "度, 金额" << sr.order.amount << "元";

    QJsonObject reply = Protocol::makeReply(Protocol::ReqStopCharge, true);
    reply.insert("order", sr.order.toJson());
    reply.insert("balance", sr.balanceAfter);
    return reply;
}

// ---------- 现场排队 ----------

QJsonObject ClientHandler::processReservePile(const QJsonObject &req)
{
    const int userId = m_userId;
    const int action = req.value("action").toInt(0);

    if (action == 1) {
        const int rid = req.value("reservationId").toInt();
        QString err;
        if (!ReservationDao::cancelByUser(rid, userId, &err, m_dbConnName))
            return Protocol::makeReply(Protocol::ReqReservePile, false,
                                       err.isEmpty() ? "取消失败, 记录不存在或已结束" : err);
        return Protocol::makeReply(Protocol::ReqReservePile, true);
    }

    const int pileId = req.value("pileId").toInt();
    if (pileId <= 0)
        return Protocol::makeReply(Protocol::ReqReservePile, false, "参数错误: 缺少pileId");

    const PileInfo pile = PileDao::getById(pileId, nullptr, m_dbConnName);
    if (pile.id == 0)
        return Protocol::makeReply(Protocol::ReqReservePile, false, "充电桩不存在");
    if (pile.status == PileFault)
        return Protocol::makeReply(Protocol::ReqReservePile, false, "该桩故障中, 无法排队");
    if (pile.status == PileIdle)
        return Protocol::makeReply(Protocol::ReqReservePile, false, "该桩当前空闲, 可直接充电");

    QString err;
    const int rid = ReservationDao::enqueue(userId, pileId, pile.stationId, &err, m_dbConnName);
    if (rid == -2)
        return Protocol::makeReply(Protocol::ReqReservePile, false, "您已在该桩排队/预约, 请勿重复操作");
    if (rid == -3)
        return Protocol::makeReply(Protocol::ReqReservePile, false, "该桩排队人数已满, 请选择其他桩");
    if (rid <= 0)
        return Protocol::makeReply(Protocol::ReqReservePile, false,
                                   err.isEmpty() ? "排队失败" : err);

    const int pos = ReservationDao::queuePosition(rid, nullptr, m_dbConnName);
    QJsonObject reply = Protocol::makeReply(Protocol::ReqReservePile, true);
    reply.insert("reservationId", rid);
    reply.insert("queuePos", pos);
    reply.insert("waiting", ReservationDao::pendingCount(pileId, nullptr, m_dbConnName));
    return reply;
}

// ---------- 时段预约 ----------

QJsonObject ClientHandler::processAppointPile(const QJsonObject &req)
{
    const int userId = m_userId;
    const int pileId = req.value("pileId").toInt();
    const QString date = req.value("reserveDate").toString();
    const QString start = req.value("reserveStart").toString();
    const QString end = req.value("reserveEnd").toString();

    if (pileId <= 0 || date.isEmpty() || start.isEmpty() || end.isEmpty())
        return Protocol::makeReply(Protocol::ReqAppointPile, false, "预约信息不完整");

    const PileInfo pile = PileDao::getById(pileId, nullptr, m_dbConnName);
    if (pile.id == 0)
        return Protocol::makeReply(Protocol::ReqAppointPile, false, "充电桩不存在");
    if (pile.status == PileFault)
        return Protocol::makeReply(Protocol::ReqAppointPile, false, "该桩故障中, 无法预约");

    QString err;
    const int rid = ReservationDao::appointCreate(userId, pileId, pile.stationId,
                                                  date, start, end, &err, m_dbConnName);
    if (rid == -2)
        return Protocol::makeReply(Protocol::ReqAppointPile, false, "该时段已被预约, 请更换时段");
    if (rid == -3)
        return Protocol::makeReply(Protocol::ReqAppointPile, false, "预约开始时间已过, 请选择未来时段");
    if (rid == -4)
        return Protocol::makeReply(Protocol::ReqAppointPile, false, "您已有该桩的有效预约, 请勿重复预约");
    if (rid <= 0)
        return Protocol::makeReply(Protocol::ReqAppointPile, false,
                                   err.isEmpty() ? "预约失败" : err);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqAppointPile, true);
    reply.insert("reservationId", rid);
    return reply;
}

QJsonObject ClientHandler::processAppointSlots(const QJsonObject &req)
{
    const int pileId = req.value("pileId").toInt();
    const QString date = req.value("date").toString();
    if (pileId <= 0 || date.isEmpty())
        return Protocol::makeReply(Protocol::ReqAppointSlots, false, "参数不完整");

    const QList<ReservationInfo> booked =
        ReservationDao::bookedSlots(pileId, date, nullptr, m_dbConnName);
    QJsonArray bookedArr;
    for (const ReservationInfo &r : booked) {
        QJsonObject b;
        b.insert("start", r.reserveStart);
        b.insert("end", r.reserveEnd);
        bookedArr.append(b);
    }

    // 08:00~22:00 每 30 分钟一个可选起点
    QJsonArray slotArr;
    for (int m = 8 * 60; m < 22 * 60; m += 30)
        slotArr.append(QString("%1:%2")
                           .arg(m / 60, 2, 10, QChar('0'))
                           .arg(m % 60, 2, 10, QChar('0')));

    QJsonObject reply = Protocol::makeReply(Protocol::ReqAppointSlots, true);
    reply.insert("slots", slotArr);
    reply.insert("booked", bookedArr);
    return reply;
}

QJsonObject ClientHandler::processMyReservations(const QJsonObject &req)
{
    Q_UNUSED(req)
    const QList<ReservationInfo> list =
        ReservationDao::myList(m_userId, nullptr, m_dbConnName);
    QJsonArray arr;
    for (const ReservationInfo &r : list)
        arr.append(r.toJson());
    QJsonObject reply = Protocol::makeReply(Protocol::ReqMyReservations, true);
    reply.insert("reservations", arr);
    return reply;
}

// ---------- 订单历史/详情/费率 ----------

QJsonObject ClientHandler::processOrderHistory(const QJsonObject &req)
{
    const int page = req.value("page").toInt(0);
    const int pageSize = req.value("pageSize").toInt(20);
    int total = 0;
    const QList<OrderInfo> orders =
        OrderDao::listByUser(m_userId, page, pageSize, &total, nullptr, m_dbConnName);
    QJsonArray arr;
    for (const OrderInfo &o : orders)
        arr.append(o.toJson());
    QJsonObject reply = Protocol::makeReply(Protocol::ReqOrderHistory, true);
    reply.insert("orders", arr);
    reply.insert("total", total);
    return reply;
}

QJsonObject ClientHandler::processOrderDetail(const QJsonObject &req)
{
    const int orderId = req.value("orderId").toInt();
    const OrderInfo o = OrderDao::getById(orderId, nullptr, m_dbConnName);
    if (o.id == 0)
        return Protocol::makeReply(Protocol::ReqOrderDetail, false, "订单不存在");
    if (o.userId != m_userId)
        return Protocol::makeReply(Protocol::ReqOrderDetail, false, "无权查看该订单");
    QJsonObject reply = Protocol::makeReply(Protocol::ReqOrderDetail, true);
    reply.insert("order", o.toJson());
    return reply;
}

QJsonObject ClientHandler::processStationFee(const QJsonObject &req)
{
    const int stationId = req.value("stationId").toInt();
    if (stationId <= 0)
        return Protocol::makeReply(Protocol::ReqStationFee, false, "参数错误: 缺少stationId");

    const QList<FeeRule> rules = PriceRuleDao::listByStation(stationId, nullptr, m_dbConnName);
    QJsonArray arr;
    for (const FeeRule &r : rules)
        arr.append(r.toJson());

    double defaultPrice = 0;
    QSqlQuery sq(QSqlDatabase::database(m_dbConnName));
    sq.prepare("SELECT price FROM station WHERE id=?");
    sq.addBindValue(stationId);
    if (sq.exec() && sq.next())
        defaultPrice = sq.value(0).toDouble();

    QJsonObject reply = Protocol::makeReply(Protocol::ReqStationFee, true);
    reply.insert("rules", arr);
    reply.insert("defaultPrice", defaultPrice);
    return reply;
}

// ---------- 发送 ----------

void ClientHandler::sendJson(const QJsonObject &obj)
{
    if (!m_socket)
        return;
    m_socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void ClientHandler::sendError(int type, const QString &text)
{
    sendJson(Protocol::makeReply(type, false, text));
}
