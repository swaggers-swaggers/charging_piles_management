#include "OrderDao.h"

#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QSqlDatabase daoDb(const QString &connName)
{
    return connName.isEmpty() ? DatabaseManager::instance().database()
                              : QSqlDatabase::database(connName);
}

// 联表字段(顺序固定, readOrder 按列序读取; 追加新列只能放末尾)
const QString kOrderFields =
    QStringLiteral("o.id, o.user_id, o.pile_id, o.station_id, o.start_time, o.end_time,"
                   " o.energy, o.amount, o.status, p.code, s.name,"
                   " o.freeze_amount, o.target_type, o.target_value, o.price_snapshot,"
                   " o.finish_type, o.cancel_reason, o.refund_amount, o.sim_minutes");

OrderInfo readOrder(QSqlQuery &q)
{
    OrderInfo r;
    r.id = q.value(0).toInt();
    r.userId = q.value(1).toInt();
    r.pileId = q.value(2).toInt();
    r.stationId = q.value(3).toInt();
    r.startTime = q.value(4).toString();
    r.endTime = q.value(5).toString();
    r.energy = q.value(6).toDouble();
    r.amount = q.value(7).toDouble();
    r.status = q.value(8).toInt();
    r.pileCode = q.value(9).toString();
    r.stationName = q.value(10).toString();
    r.freezeAmount = q.value(11).toDouble();
    r.targetType = q.value(12).toInt();
    r.targetValue = q.value(13).toDouble();
    r.priceSnapshot = q.value(14).toDouble();
    r.finishType = q.value(15).toInt();
    r.cancelReason = q.value(16).toString();
    r.refundAmount = q.value(17).toDouble();
    r.simMinutes = q.value(18).toInt();
    return r;
}
} // namespace

int OrderDao::create(int userId, int pileId, int stationId,
                     double priceSnapshot, double freezeAmount,
                     int targetType, double targetValue,
                     QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("INSERT INTO charge_order(user_id, pile_id, station_id, start_time,"
              " price_snapshot, freeze_amount, target_type, target_value, status)"
              " VALUES(?,?,?,datetime('now','localtime'),?,?,?,?,0)");
    q.addBindValue(userId);
    q.addBindValue(pileId);
    q.addBindValue(stationId);
    q.addBindValue(priceSnapshot);
    q.addBindValue(freezeAmount);
    q.addBindValue(targetType);
    q.addBindValue(targetValue);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

OrderInfo OrderDao::getById(int id, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT " + kOrderFields +
              " FROM charge_order o"
              " JOIN pile p ON o.pile_id=p.id"
              " JOIN station s ON o.station_id=s.id WHERE o.id=?");
    q.addBindValue(id);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return OrderInfo();
    }
    if (!q.next())
        return OrderInfo();
    return readOrder(q);
}

OrderInfo OrderDao::getUnfinishedByUser(int userId, bool *hasOrder, QString *errMsg,
                                        const QString &connName)
{
    if (hasOrder)
        *hasOrder = false;
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT " + kOrderFields +
              " FROM charge_order o"
              " JOIN pile p ON o.pile_id=p.id"
              " JOIN station s ON o.station_id=s.id"
              " WHERE o.user_id=? AND o.status=0 ORDER BY o.id DESC LIMIT 1");
    q.addBindValue(userId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return OrderInfo();
    }
    if (!q.next())
        return OrderInfo();
    if (hasOrder)
        *hasOrder = true;
    return readOrder(q);
}

OrderDao::OrderContext OrderDao::getContext(int orderId, QString *errMsg,
                                           const QString &connName)
{
    OrderContext ctx;
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT o.id, o.user_id, o.pile_id, p.status, p.power, s.price,"
              " o.energy, o.amount, o.sim_minutes, o.freeze_amount,"
              " o.target_type, o.target_value, o.price_snapshot, u.balance"
              " FROM charge_order o"
              " JOIN pile p ON o.pile_id=p.id"
              " JOIN station s ON o.station_id=s.id"
              " JOIN user u ON o.user_id=u.id"
              " WHERE o.id=?");
    q.addBindValue(orderId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return ctx;
    }
    if (!q.next())
        return ctx;
    ctx.exists = true;
    ctx.orderId = q.value(0).toInt();
    ctx.userId = q.value(1).toInt();
    ctx.pileId = q.value(2).toInt();
    ctx.pileStatus = q.value(3).toInt();
    ctx.power = q.value(4).toDouble();
    ctx.price = q.value(5).toDouble();
    ctx.energy = q.value(6).toDouble();
    ctx.amount = q.value(7).toDouble();
    ctx.simMinutes = q.value(8).toInt();
    ctx.freezeAmount = q.value(9).toDouble();
    ctx.targetType = q.value(10).toInt();
    ctx.targetValue = q.value(11).toDouble();
    ctx.priceSnapshot = q.value(12).toDouble();
    ctx.userBalance = q.value(13).toDouble();
    return ctx;
}

bool OrderDao::updateProgress(int orderId, double energy, double amount, int simMinutes,
                              QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_order SET energy=?, amount=?, sim_minutes=? WHERE id=?");
    q.addBindValue(energy);
    q.addBindValue(amount);
    q.addBindValue(simMinutes);
    q.addBindValue(orderId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool OrderDao::finishWithType(int orderId, double energy, double amount, int simMinutes,
                              int finishType, const QString &reason,
                              QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_order SET end_time=datetime('now','localtime'),"
              " energy=?, amount=?, sim_minutes=?, status=1, finish_type=?, cancel_reason=?"
              " WHERE id=?");
    q.addBindValue(energy);
    q.addBindValue(amount);
    q.addBindValue(simMinutes);
    q.addBindValue(finishType);
    q.addBindValue(reason);
    q.addBindValue(orderId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool OrderDao::addRefund(int orderId, double amount,
                         QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_order SET refund_amount=refund_amount+? WHERE id=?");
    q.addBindValue(amount);
    q.addBindValue(orderId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return true;
}

QList<OrderInfo> OrderDao::listActive(QString *errMsg, const QString &connName)
{
    QList<OrderInfo> list;
    QSqlQuery q(daoDb(connName));
    if (!q.exec("SELECT " + kOrderFields +
                " FROM charge_order o"
                " JOIN pile p ON o.pile_id=p.id"
                " JOIN station s ON o.station_id=s.id"
                " WHERE o.status=0 ORDER BY o.id")) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readOrder(q));
    return list;
}

QList<OrderInfo> OrderDao::listByUser(int userId, int page, int pageSize, int *total,
                                      QString *errMsg, const QString &connName)
{
    QList<OrderInfo> list;
    if (total)
        *total = 0;
    if (pageSize <= 0)
        pageSize = 20;
    if (page < 0)
        page = 0;

    QSqlDatabase db = daoDb(connName);
    QSqlQuery cnt(db);
    cnt.prepare("SELECT COUNT(*) FROM charge_order WHERE user_id=?");
    cnt.addBindValue(userId);
    if (cnt.exec() && cnt.next() && total)
        *total = cnt.value(0).toInt();

    QSqlQuery q(db);
    q.prepare("SELECT " + kOrderFields +
              " FROM charge_order o"
              " JOIN pile p ON o.pile_id=p.id"
              " JOIN station s ON o.station_id=s.id"
              " WHERE o.user_id=? ORDER BY o.id DESC LIMIT ? OFFSET ?");
    q.addBindValue(userId);
    q.addBindValue(pageSize);
    q.addBindValue(page * pageSize);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readOrder(q));
    return list;
}

QList<OrderInfo> OrderDao::listAll(int statusFilter, QString *errMsg, const QString &connName)
{
    QList<OrderInfo> list;
    QSqlQuery q(daoDb(connName));
    if (statusFilter < 0) {
        if (!q.exec("SELECT " + kOrderFields +
                    " FROM charge_order o"
                    " JOIN pile p ON o.pile_id=p.id"
                    " JOIN station s ON o.station_id=s.id"
                    " ORDER BY o.id DESC LIMIT 500")) {
            if (errMsg)
                *errMsg = q.lastError().text();
            return list;
        }
    } else {
        q.prepare("SELECT " + kOrderFields +
                  " FROM charge_order o"
                  " JOIN pile p ON o.pile_id=p.id"
                  " JOIN station s ON o.station_id=s.id"
                  " WHERE o.status=? ORDER BY o.id DESC LIMIT 500");
        q.addBindValue(statusFilter);
        if (!q.exec()) {
            if (errMsg)
                *errMsg = q.lastError().text();
            return list;
        }
    }
    while (q.next())
        list.append(readOrder(q));
    return list;
}

bool OrderDao::salesSummary(double *today, double *month, double *total,
                            QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    if (!q.exec("SELECT"
                " COALESCE(SUM(CASE WHEN date(end_time)=date('now','localtime') THEN amount END),0),"
                " COALESCE(SUM(CASE WHEN end_time>=datetime('now','localtime','-30 days') THEN amount END),0),"
                " COALESCE(SUM(amount),0)"
                " FROM charge_order WHERE status=1")) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    if (!q.next())
        return false;
    if (today) *today = q.value(0).toDouble();
    if (month) *month = q.value(1).toDouble();
    if (total) *total = q.value(2).toDouble();
    return true;
}

QVector<QPair<QString, double>> OrderDao::dailyRevenue(int days, QString *errMsg,
                                                       const QString &connName)
{
    QVector<QPair<QString, double>> list;
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT date(end_time) d, COALESCE(SUM(amount),0) FROM charge_order"
              " WHERE status=1 AND end_time>=datetime('now','localtime',?)"
              " GROUP BY d ORDER BY d");
    q.addBindValue(QString("-%1 days").arg(days));
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append({ q.value(0).toString(), q.value(1).toDouble() });
    return list;
}

QList<QPair<QString, double>> OrderDao::stationRevenue(QString *errMsg,
                                                       const QString &connName)
{
    QList<QPair<QString, double>> list;
    QSqlQuery q(daoDb(connName));
    if (!q.exec("SELECT s.name, COALESCE(SUM(o.amount),0)"
                " FROM charge_order o JOIN station s ON o.station_id=s.id"
                " WHERE o.status=1 GROUP BY o.station_id ORDER BY SUM(o.amount) DESC")) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append({ q.value(0).toString(), q.value(1).toDouble() });
    return list;
}
