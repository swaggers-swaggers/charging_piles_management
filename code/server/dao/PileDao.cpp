#include "PileDao.h"

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

const QString kPileSelect =
    QStringLiteral("SELECT p.id, p.station_id, p.code, p.type, p.power, p.status,"
                   " p.total_count, p.total_duration, s.name"
                   " FROM pile p JOIN station s ON p.station_id=s.id");

PileInfo readPile(QSqlQuery &q)
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
} // namespace

QList<PileInfo> PileDao::listAll(const QString &connName)
{
    QList<PileInfo> list;
    QSqlQuery q(daoDb(connName));
    if (!q.exec(kPileSelect + " ORDER BY p.code"))
        return list;
    while (q.next())
        list.append(readPile(q));
    return list;
}

QList<PileInfo> PileDao::listByStation(int stationId, const QString &connName)
{
    QList<PileInfo> list;
    QSqlQuery q(daoDb(connName));
    q.prepare(kPileSelect + " WHERE p.station_id=? ORDER BY p.code");
    q.addBindValue(stationId);
    if (!q.exec())
        return list;
    while (q.next())
        list.append(readPile(q));
    return list;
}

PileInfo PileDao::getById(int id, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare(kPileSelect + " WHERE p.id=?");
    q.addBindValue(id);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return PileInfo();
    }
    if (!q.next())
        return PileInfo();
    return readPile(q);
}

bool PileDao::setStatus(int id, int status, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE pile SET status=? WHERE id=?");
    q.addBindValue(status);
    q.addBindValue(id);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool PileDao::restart(int id, QString *errMsg, const QString &connName)
{
    // 远程重启: 故障桩恢复为闲置
    return setStatus(id, PileIdle, errMsg, connName);
}

bool PileDao::addUsage(int id, int addedMinutes, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE pile SET total_count=total_count+1, total_duration=total_duration+? WHERE id=?");
    q.addBindValue(addedMinutes);
    q.addBindValue(id);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool PileDao::statusCounts(int *idle, int *inUse, int *fault, QString *errMsg,
                           const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    if (!q.exec("SELECT status, COUNT(*) FROM pile GROUP BY status")) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    if (idle) *idle = 0;
    if (inUse) *inUse = 0;
    if (fault) *fault = 0;
    while (q.next()) {
        const int status = q.value(0).toInt();
        const int cnt = q.value(1).toInt();
        if (status == PileIdle && idle) *idle = cnt;
        else if (status == PileInUse && inUse) *inUse = cnt;
        else if (status == PileFault && fault) *fault = cnt;
    }
    return true;
}

bool PileDao::acquire(int pileId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE pile SET status=1 WHERE id=? AND status=0");
    q.addBindValue(pileId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() == 1;
}

bool PileDao::release(int pileId, int usedMinutes, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE pile SET status=CASE WHEN status=2 THEN 2 ELSE 0 END,"
              " total_count=total_count+1,"
              " total_duration=total_duration+? WHERE id=?");
    q.addBindValue(usedMinutes);
    q.addBindValue(pileId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return false;
    }
    return true;
}
