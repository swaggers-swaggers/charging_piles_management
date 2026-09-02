#include "OrderDao.h"

#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static OrderInfo readOrder(const QSqlQuery &q)
{
    OrderInfo o;
    o.id = q.value(0).toInt();
    o.userId = q.value(1).toInt();
    o.pileId = q.value(2).toInt();
    o.stationId = q.value(3).toInt();
    o.startTime = q.value(4).toString();
    o.endTime = q.value(5).toString();
    o.energy = q.value(6).toDouble();
    o.amount = q.value(7).toDouble();
    o.status = q.value(8).toInt();
    o.pileCode = q.value(9).toString();
    o.stationName = q.value(10).toString();
    return o;
}

static const char *kOrderFields =
    "o.id, o.user_id, o.pile_id, o.station_id, o.start_time, o.end_time,"
    " o.energy, o.amount, o.status, p.code, s.name";
static const char *kOrderFrom =
    " FROM charge_order o"
    " JOIN pile p ON p.id = o.pile_id"
    " JOIN station s ON s.id = o.station_id";

bool OrderDao::create(int userId, int pileId, int stationId, const QString &startTime,
                      int *orderId, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("INSERT INTO charge_order (user_id, pile_id, station_id, start_time,"
                  " energy, amount, status) VALUES (:u, :p, :s, :t, 0, 0, 0)");
    query.bindValue(":u", userId);
    query.bindValue(":p", pileId);
    query.bindValue(":s", stationId);
    query.bindValue(":t", startTime);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "创建订单失败: " + query.lastError().text();
        return false;
    }
    if (orderId)
        *orderId = query.lastInsertId().toInt();
    return true;
}

bool OrderDao::getById(int orderId, OrderInfo *out, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare(QString(kOrderFields).prepend("SELECT ") + kOrderFrom + " WHERE o.id = :id");
    query.bindValue(":id", orderId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询订单失败: " + query.lastError().text();
        return false;
    }
    if (!query.next()) {
        if (errMsg)
            *errMsg = "订单不存在";
        return false;
    }
    if (out)
        *out = readOrder(query);
    return true;
}

bool OrderDao::getUnfinishedByUser(int userId, OrderInfo *out, bool *has,
                                   QString *errMsg, const QString &connName)
{
    if (has)
        *has = false;
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare(QString(kOrderFields).prepend("SELECT ") + kOrderFrom +
                  " WHERE o.user_id = :u AND o.status = 0 ORDER BY o.id DESC LIMIT 1");
    query.bindValue(":u", userId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询未完成订单失败: " + query.lastError().text();
        return false;
    }
    if (query.next()) {
        if (has)
            *has = true;
        if (out)
            *out = readOrder(query);
    }
    return true;
}

bool OrderDao::getContext(int orderId, OrderContext *out, QString *errMsg,
                          const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare(QString(kOrderFields).prepend("SELECT ") + ", p.power, s.price" +
                  kOrderFrom + " WHERE o.id = :id");
    query.bindValue(":id", orderId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询订单详情失败: " + query.lastError().text();
        return false;
    }
    if (!query.next()) {
        if (errMsg)
            *errMsg = "订单不存在";
        return false;
    }
    if (out) {
        out->order = readOrder(query);
        out->power = query.value(11).toDouble();
        out->price = query.value(12).toDouble();
    }
    return true;
}

bool OrderDao::updateProgress(int orderId, double energy, double amount, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE charge_order SET energy = :e, amount = :a WHERE id = :id");
    query.bindValue(":e", energy);
    query.bindValue(":a", amount);
    query.bindValue(":id", orderId);
    return query.exec();
}

bool OrderDao::finish(int orderId, const QString &endTime, double energy, double amount,
                      QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE charge_order SET end_time = :t, energy = :e, amount = :a,"
                  " status = 1 WHERE id = :id");
    query.bindValue(":t", endTime);
    query.bindValue(":e", energy);
    query.bindValue(":a", amount);
    query.bindValue(":id", orderId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "完成订单失败: " + query.lastError().text();
        return false;
    }
    return true;
}

bool OrderDao::salesSummary(double *today, double *month, double *total,
                            QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    const QString sql =
        "SELECT"
        " COALESCE(SUM(CASE WHEN date(start_time) = date('now','localtime') THEN amount ELSE 0 END), 0),"
        " COALESCE(SUM(CASE WHEN strftime('%Y-%m', start_time) = strftime('%Y-%m','now','localtime') THEN amount ELSE 0 END), 0),"
        " COALESCE(SUM(amount), 0)"
        " FROM charge_order WHERE status = 1";
    if (!query.exec(sql)) {
        if (errMsg)
            *errMsg = "统计营收失败: " + query.lastError().text();
        return false;
    }
    if (query.next()) {
        if (today)
            *today = query.value(0).toDouble();
        if (month)
            *month = query.value(1).toDouble();
        if (total)
            *total = query.value(2).toDouble();
    }
    return true;
}

QVector<QPair<QString, double>> OrderDao::dailyRevenue(int lastDays, QString *errMsg,
                                                       const QString &connName)
{
    QVector<QPair<QString, double>> result;

    // 先从库里取有数据的日子
    QHash<QString, double> byDate;
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("SELECT date(start_time) AS d, SUM(amount) FROM charge_order"
                  " WHERE status = 1 AND date(start_time) >= date('now','localtime', :off)"
                  " GROUP BY d");
    query.bindValue(":off", QString("-%1 days").arg(lastDays - 1));
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "统计每日营收失败: " + query.lastError().text();
        return result;
    }
    while (query.next())
        byDate.insert(query.value(0).toString(), query.value(1).toDouble());

    // 按日期升序补齐缺失日期为 0
    const QDate today = QDate::currentDate();
    for (int i = lastDays - 1; i >= 0; --i) {
        const QString key = today.addDays(-i).toString("yyyy-MM-dd");
        result.append(qMakePair(key, byDate.value(key, 0.0)));
    }
    return result;
}

QList<QPair<QString, double>> OrderDao::stationRevenue(QString *errMsg, const QString &connName)
{
    QList<QPair<QString, double>> result;
    QSqlQuery query(QSqlDatabase::database(connName));
    const QString sql =
        "SELECT s.name, COALESCE(SUM(o.amount), 0) AS rev FROM station s"
        " LEFT JOIN charge_order o ON o.station_id = s.id AND o.status = 1"
        " GROUP BY s.id ORDER BY rev DESC";
    if (!query.exec(sql)) {
        if (errMsg)
            *errMsg = "统计各站营收失败: " + query.lastError().text();
        return result;
    }
    while (query.next())
        result.append(qMakePair(query.value(0).toString(), query.value(1).toDouble()));
    return result;
}
