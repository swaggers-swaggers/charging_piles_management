#include "ReservationDao.h"

#include "DatabaseManager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QSqlDatabase daoDb(const QString &connName)
{
    return connName.isEmpty() ? DatabaseManager::instance().database()
                              : QSqlDatabase::database(connName);
}

const QString kResFields =
    QStringLiteral("r.id, r.user_id, r.pile_id, r.station_id, r.type, r.create_time,"
                   " r.assign_time, r.expire_time, r.reserve_date, r.reserve_start,"
                   " r.reserve_end, r.status, p.code, s.name, u.phone_masked"
                   " FROM charge_reservation r"
                   " JOIN pile p ON r.pile_id=p.id"
                   " JOIN station s ON r.station_id=s.id"
                   " JOIN user u ON r.user_id=u.id");

ReservationInfo readRes(QSqlQuery &q)
{
    ReservationInfo r;
    r.id = q.value(0).toInt();
    r.userId = q.value(1).toInt();
    r.pileId = q.value(2).toInt();
    r.stationId = q.value(3).toInt();
    r.type = q.value(4).toInt();
    r.createTime = q.value(5).toString();
    r.assignTime = q.value(6).toString();
    r.expireTime = q.value(7).toString();
    r.reserveDate = q.value(8).toString();
    r.reserveStart = q.value(9).toString();
    r.reserveEnd = q.value(10).toString();
    r.status = q.value(11).toInt();
    r.pileCode = q.value(12).toString();
    r.stationName = q.value(13).toString();
    r.phoneMasked = q.value(14).toString();
    return r;
}
} // namespace

int ReservationDao::enqueue(int userId, int pileId, int stationId,
                            QString *errMsg, const QString &connName)
{
    QSqlDatabase db = daoDb(connName);

    QSqlQuery dup(db);
    dup.prepare("SELECT COUNT(*) FROM charge_reservation"
                " WHERE user_id=? AND pile_id=? AND type=0 AND status IN (0,1)");
    dup.addBindValue(userId);
    dup.addBindValue(pileId);
    if (!dup.exec()) {
        if (errMsg) *errMsg = dup.lastError().text();
        return -1;
    }
    if (dup.next() && dup.value(0).toInt() > 0)
        return -2;

    QSqlQuery full(db);
    full.prepare("SELECT COUNT(*) FROM charge_reservation WHERE pile_id=? AND type=0 AND status=0");
    full.addBindValue(pileId);
    if (!full.exec()) {
        if (errMsg) *errMsg = full.lastError().text();
        return -1;
    }
    if (full.next() && full.value(0).toInt() >= 10)
        return -3;

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO charge_reservation(user_id, pile_id, station_id, type, status)"
                " VALUES(?,?,?,0,0)");
    ins.addBindValue(userId);
    ins.addBindValue(pileId);
    ins.addBindValue(stationId);
    if (!ins.exec()) {
        if (errMsg) *errMsg = ins.lastError().text();
        return -1;
    }
    return ins.lastInsertId().toInt();
}

bool ReservationDao::cancelByUser(int reservationId, int userId,
                                  QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_reservation SET status=2 WHERE id=? AND user_id=? AND status IN (0,1)");
    q.addBindValue(reservationId);
    q.addBindValue(userId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() >= 1;
}

bool ReservationDao::cancelByAdmin(int reservationId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_reservation SET status=2 WHERE id=? AND status IN (0,1)");
    q.addBindValue(reservationId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() >= 1;
}

int ReservationDao::queuePosition(int reservationId, QString *errMsg, const QString &connName)
{
    QSqlDatabase db = daoDb(connName);
    QSqlQuery self(db);
    self.prepare("SELECT pile_id, status FROM charge_reservation WHERE id=?");
    self.addBindValue(reservationId);
    if (!self.exec() || !self.next()) {
        if (errMsg && self.lastError().isValid()) *errMsg = self.lastError().text();
        return -1;
    }
    const int pileId = self.value(0).toInt();
    const int status = self.value(1).toInt();
    if (status == 1)
        return 0;   // 已分配 = 轮到自己
    if (status != 0)
        return -1;

    QSqlQuery pos(db);
    pos.prepare("SELECT COUNT(*) FROM charge_reservation"
                " WHERE pile_id=? AND type=0 AND status=0 AND id<=?");
    pos.addBindValue(pileId);
    pos.addBindValue(reservationId);
    if (!pos.exec()) {
        if (errMsg) *errMsg = pos.lastError().text();
        return -1;
    }
    return pos.next() ? pos.value(0).toInt() : -1;
}

int ReservationDao::pendingCount(int pileId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT COUNT(*) FROM charge_reservation WHERE pile_id=? AND type=0 AND status=0");
    q.addBindValue(pileId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return 0;
    }
    return q.next() ? q.value(0).toInt() : 0;
}

ReservationInfo ReservationDao::nextPending(int pileId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT " + kResFields +
              " WHERE r.pile_id=? AND r.type=0 AND r.status=0 ORDER BY r.id LIMIT 1");
    q.addBindValue(pileId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return ReservationInfo();
    }
    if (!q.next())
        return ReservationInfo();
    ReservationInfo r = readRes(q);
    r.queuePos = 1;
    return r;
}

bool ReservationDao::markAssigned(int reservationId, int confirmSec,
                                  QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_reservation SET status=1,"
              " assign_time=datetime('now','localtime'),"
              " expire_time=datetime('now','localtime',?)"
              " WHERE id=? AND status=0");
    q.addBindValue(QString("+%1 seconds").arg(confirmSec));
    q.addBindValue(reservationId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() == 1;
}

bool ReservationDao::markFulfilled(int reservationId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_reservation SET status=4 WHERE id=? AND status IN (0,1)");
    q.addBindValue(reservationId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() >= 1;
}

bool ReservationDao::setStatus(int reservationId, int status,
                               QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_reservation SET status=? WHERE id=?");
    q.addBindValue(status);
    q.addBindValue(reservationId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return false;
    }
    return true;
}

int ReservationDao::appointCreate(int userId, int pileId, int stationId,
                                  const QString &date, const QString &start,
                                  const QString &end, QString *errMsg,
                                  const QString &connName)
{
    QSqlDatabase db = daoDb(connName);

    // 时段已过校验
    const QDateTime startDt = QDateTime::fromString(date + " " + start, "yyyy-MM-dd HH:mm");
    if (!startDt.isValid() || startDt <= QDateTime::currentDateTime().addSecs(60))
        return -3;

    QSqlQuery dup(db);
    dup.prepare("SELECT COUNT(*) FROM charge_reservation"
                " WHERE user_id=? AND pile_id=? AND type=1 AND status IN (0,1)");
    dup.addBindValue(userId);
    dup.addBindValue(pileId);
    if (!dup.exec()) {
        if (errMsg) *errMsg = dup.lastError().text();
        return -1;
    }
    if (dup.next() && dup.value(0).toInt() > 0)
        return -4;

    // 时段重叠: 已存在 [s,e) 与新 [start,end) 相交 ⇔ s < end AND e > start
    QSqlQuery conflict(db);
    conflict.prepare("SELECT COUNT(*) FROM charge_reservation"
                     " WHERE pile_id=? AND type=1 AND status IN (0,1)"
                     " AND reserve_date=? AND reserve_start<? AND reserve_end>?");
    conflict.addBindValue(pileId);
    conflict.addBindValue(date);
    conflict.addBindValue(end);
    conflict.addBindValue(start);
    if (!conflict.exec()) {
        if (errMsg) *errMsg = conflict.lastError().text();
        return -1;
    }
    if (conflict.next() && conflict.value(0).toInt() > 0)
        return -2;

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO charge_reservation(user_id, pile_id, station_id, type,"
                " reserve_date, reserve_start, reserve_end, status)"
                " VALUES(?,?,?,1,?,?,?,0)");
    ins.addBindValue(userId);
    ins.addBindValue(pileId);
    ins.addBindValue(stationId);
    ins.addBindValue(date);
    ins.addBindValue(start);
    ins.addBindValue(end);
    if (!ins.exec()) {
        if (errMsg) *errMsg = ins.lastError().text();
        return -1;
    }
    return ins.lastInsertId().toInt();
}

QList<ReservationInfo> ReservationDao::bookedSlots(int pileId, const QString &date,
                                                   QString *errMsg, const QString &connName)
{
    QList<ReservationInfo> list;
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT " + kResFields +
              " WHERE r.pile_id=? AND r.type=1 AND r.status IN (0,1)"
              " AND r.reserve_date=? ORDER BY r.reserve_start");
    q.addBindValue(pileId);
    q.addBindValue(date);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readRes(q));
    return list;
}

ReservationInfo ReservationDao::fulfillTodayAppoint(int userId, int pileId,
                                                    QString *errMsg, const QString &connName)
{
    QSqlDatabase db = daoDb(connName);
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QSqlQuery q(db);
    q.prepare("SELECT " + kResFields +
              " WHERE r.user_id=? AND r.pile_id=? AND r.type=1 AND r.status=0"
              " AND r.reserve_date=? ORDER BY r.id LIMIT 1");
    q.addBindValue(userId);
    q.addBindValue(pileId);
    q.addBindValue(today);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return ReservationInfo();
    }
    if (!q.next())
        return ReservationInfo();
    ReservationInfo r = readRes(q);
    QSqlQuery up(db);
    up.prepare("UPDATE charge_reservation SET status=4 WHERE id=?");
    up.addBindValue(r.id);
    up.exec();
    return r;
}

QList<ReservationInfo> ReservationDao::myList(int userId, QString *errMsg,
                                              const QString &connName)
{
    QList<ReservationInfo> list;
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT " + kResFields +
              " WHERE r.user_id=? ORDER BY r.id DESC LIMIT 50");
    q.addBindValue(userId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return list;
    }
    while (q.next()) {
        ReservationInfo r = readRes(q);
        if (r.type == 0 && r.status == 0)
            r.queuePos = queuePosition(r.id, nullptr, connName);
        list.append(r);
    }
    return list;
}

QList<ReservationInfo> ReservationDao::listAll(int statusFilter, int typeFilter,
                                               QString *errMsg, const QString &connName)
{
    QList<ReservationInfo> list;
    QStringList where;
    if (statusFilter >= 0)
        where << "r.status=?";
    if (typeFilter >= 0)
        where << "r.type=?";
    const QString sql = "SELECT " + kResFields +
                        (where.isEmpty() ? "" : " WHERE " + where.join(" AND ")) +
                        " ORDER BY r.id DESC LIMIT 500";
    QSqlQuery q(daoDb(connName));
    if (statusFilter >= 0)
        q.addBindValue(statusFilter);
    if (typeFilter >= 0)
        q.addBindValue(typeFilter);
    if (!q.exec(sql)) {
        if (errMsg) *errMsg = q.lastError().text();
        return list;
    }
    while (q.next()) {
        ReservationInfo r = readRes(q);
        if (r.type == 0 && r.status == 0)
            r.queuePos = queuePosition(r.id, nullptr, connName);
        list.append(r);
    }
    return list;
}

ReservationInfo ReservationDao::getById(int id, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT " + kResFields + " WHERE r.id=?");
    q.addBindValue(id);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return ReservationInfo();
    }
    if (!q.next())
        return ReservationInfo();
    return readRes(q);
}

QList<ReservationInfo> ReservationDao::listAssignedExpired(QString *errMsg,
                                                           const QString &connName)
{
    QList<ReservationInfo> list;
    QSqlQuery q(daoDb(connName));
    if (!q.exec("SELECT " + kResFields +
                " WHERE r.type=0 AND r.status=1"
                " AND r.expire_time IS NOT NULL AND r.expire_time<=datetime('now','localtime')"
                " ORDER BY r.id")) {
        if (errMsg) *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readRes(q));
    return list;
}

QList<ReservationInfo> ReservationDao::listAppointRemindDue(QString *errMsg,
                                                            const QString &connName)
{
    QList<ReservationInfo> list;
    QSqlQuery q(daoDb(connName));
    // 开始前 10 分钟进入提醒窗口, 且尚未到开始时间, 未提醒过
    if (!q.exec("SELECT " + kResFields +
                " WHERE r.type=1 AND r.status=0 AND r.remind_sent=0"
                " AND datetime(r.reserve_date || ' ' || r.reserve_start || ':00','-10 minutes')"
                "   <= datetime('now','localtime')"
                " AND datetime(r.reserve_date || ' ' || r.reserve_start || ':00')"
                "   > datetime('now','localtime')"
                " ORDER BY r.id")) {
        if (errMsg) *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readRes(q));
    return list;
}

QList<ReservationInfo> ReservationDao::listAppointExpired(QString *errMsg,
                                                          const QString &connName)
{
    QList<ReservationInfo> list;
    QSqlQuery q(daoDb(connName));
    if (!q.exec("SELECT " + kResFields +
                " WHERE r.type=1 AND r.status=0"
                " AND datetime(r.reserve_date || ' ' || r.reserve_end || ':00','+15 minutes')"
                "   <= datetime('now','localtime')"
                " ORDER BY r.id")) {
        if (errMsg) *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readRes(q));
    return list;
}

bool ReservationDao::markRemindSent(int reservationId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(daoDb(connName));
    q.prepare("UPDATE charge_reservation SET remind_sent=1 WHERE id=?");
    q.addBindValue(reservationId);
    if (!q.exec()) {
        if (errMsg) *errMsg = q.lastError().text();
        return false;
    }
    return true;
}
