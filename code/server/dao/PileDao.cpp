#include "PileDao.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static PileInfo readPile(const QSqlQuery &q)
{
    PileInfo p;
    p.id = q.value(0).toInt();
    p.stationId = q.value(1).toInt();
    p.code = q.value(2).toString();
    p.type = q.value(3).toInt();
    p.power = q.value(4).toDouble();
    p.status = q.value(5).toInt();
    p.totalCount = q.value(6).toInt();
    p.totalDuration = q.value(7).toInt();
    p.stationName = q.value(8).toString();
    return p;
}

static const char *kPileSelect =
    "SELECT p.id, p.station_id, p.code, p.type, p.power, p.status, p.total_count,"
    " p.total_duration, s.name FROM pile p JOIN station s ON s.id = p.station_id";

QList<PileInfo> PileDao::listAll(const QString &connName)
{
    QList<PileInfo> piles;
    QSqlQuery query(QSqlDatabase::database(connName));
    if (!query.exec(QString(kPileSelect) + " ORDER BY p.id"))
        return piles;
    while (query.next())
        piles.append(readPile(query));
    return piles;
}

QList<PileInfo> PileDao::listByStation(int stationId, const QString &connName)
{
    QList<PileInfo> piles;
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare(QString(kPileSelect) + " WHERE p.station_id = :sid ORDER BY p.id");
    query.bindValue(":sid", stationId);
    if (!query.exec())
        return piles;
    while (query.next())
        piles.append(readPile(query));
    return piles;
}

bool PileDao::getById(int pileId, PileInfo *out, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare(QString(kPileSelect) + " WHERE p.id = :id");
    query.bindValue(":id", pileId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询电桩失败: " + query.lastError().text();
        return false;
    }
    if (!query.next()) {
        if (errMsg)
            *errMsg = "电桩不存在";
        return false;
    }
    if (out)
        *out = readPile(query);
    return true;
}

bool PileDao::setStatus(int pileId, int status, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE pile SET status = :s WHERE id = :id");
    query.bindValue(":s", status);
    query.bindValue(":id", pileId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "更新电桩状态失败: " + query.lastError().text();
        return false;
    }
    return true;
}

bool PileDao::restart(int pileId, QString *errMsg, const QString &connName)
{
    PileInfo pile;
    if (!getById(pileId, &pile, errMsg, connName))
        return false;

    if (pile.status == PileInUse) {
        if (errMsg)
            *errMsg = "电桩正在使用中, 暂不能重启";
        return false;
    }

    return setStatus(pileId, PileIdle, errMsg, connName);
}

bool PileDao::addUsage(int pileId, int durationMinutes, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE pile SET total_count = total_count + 1,"
                  " total_duration = total_duration + :d WHERE id = :id");
    query.bindValue(":d", durationMinutes);
    query.bindValue(":id", pileId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "更新电桩统计失败: " + query.lastError().text();
        return false;
    }
    return true;
}

bool PileDao::statusCounts(int *idle, int *inUse, int *fault, QString *errMsg,
                           const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    if (!query.exec("SELECT status, COUNT(*) FROM pile GROUP BY status")) {
        if (errMsg)
            *errMsg = "统计电桩状态失败: " + query.lastError().text();
        return false;
    }
    if (idle)
        *idle = 0;
    if (inUse)
        *inUse = 0;
    if (fault)
        *fault = 0;
    while (query.next()) {
        const int status = query.value(0).toInt();
        const int count = query.value(1).toInt();
        if (status == PileIdle && idle)
            *idle = count;
        else if (status == PileInUse && inUse)
            *inUse = count;
        else if (status == PileFault && fault)
            *fault = count;
    }
    return true;
}
