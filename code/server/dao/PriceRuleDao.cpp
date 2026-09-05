#include "PriceRuleDao.h"

#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTime>

namespace {
QSqlDatabase daoDb(const QString &connName)
{
    return connName.isEmpty() ? DatabaseManager::instance().database()
                              : QSqlDatabase::database(connName);
}

FeeRule readRule(QSqlQuery &q)
{
    FeeRule r;
    r.id = q.value(0).toInt();
    r.stationId = q.value(1).toInt();
    r.period = q.value(2).toInt();
    r.startTime = q.value(3).toString();
    r.endTime = q.value(4).toString();
    r.price = q.value(5).toDouble();
    r.serviceFee = q.value(6).toDouble();
    return r;
}
} // namespace

int PriceRuleDao::hhmmToMinutes(const QString &hhmm)
{
    const QStringList parts = hhmm.split(':');
    if (parts.size() < 2)
        return 0;
    return parts[0].toInt() * 60 + parts[1].toInt();
}

QList<FeeRule> PriceRuleDao::listByStation(int stationId, QString *errMsg,
                                           const QString &connName)
{
    QList<FeeRule> list;
    QSqlQuery q(daoDb(connName));
    q.prepare("SELECT id, station_id, period, start_time, end_time, price, service_fee"
              " FROM price_rule WHERE station_id=? ORDER BY start_time");
    q.addBindValue(stationId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readRule(q));
    return list;
}

bool PriceRuleDao::currentPrice(int stationId, double *totalPrice, double *serviceFee,
                                const QTime &when, QString *errMsg, const QString &connName)
{
    const QTime t = when.isValid() ? when : QTime::currentTime();
    const int nowMin = t.hour() * 60 + t.minute();

    const QList<FeeRule> rules = listByStation(stationId, errMsg, connName);
    if (rules.isEmpty())
        return false;

    for (const FeeRule &r : rules) {
        const int s = hhmmToMinutes(r.startTime);
        const int e = hhmmToMinutes(r.endTime);
        if (nowMin >= s && nowMin < e) {
            if (totalPrice)
                *totalPrice = r.price + r.serviceFee;
            if (serviceFee)
                *serviceFee = r.serviceFee;
            return true;
        }
    }
    // 未命中任何段(理论上不应发生), 回退第一条
    if (totalPrice)
        *totalPrice = rules.first().price + rules.first().serviceFee;
    if (serviceFee)
        *serviceFee = rules.first().serviceFee;
    return true;
}

bool PriceRuleDao::replaceForStation(int stationId, const QList<FeeRule> &rules,
                                     QString *errMsg, const QString &connName)
{
    QSqlDatabase db = daoDb(connName);
    if (!db.transaction()) {
        if (errMsg)
            *errMsg = db.lastError().text();
        return false;
    }
    QSqlQuery del(db);
    del.prepare("DELETE FROM price_rule WHERE station_id=?");
    del.addBindValue(stationId);
    if (!del.exec()) {
        db.rollback();
        if (errMsg)
            *errMsg = del.lastError().text();
        return false;
    }
    for (const FeeRule &r : rules) {
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO price_rule(station_id, period, start_time, end_time, price, service_fee)"
                    " VALUES(?,?,?,?,?,?)");
        ins.addBindValue(stationId);
        ins.addBindValue(r.period);
        ins.addBindValue(r.startTime);
        ins.addBindValue(r.endTime);
        ins.addBindValue(r.price);
        ins.addBindValue(r.serviceFee);
        if (!ins.exec()) {
            db.rollback();
            if (errMsg)
                *errMsg = ins.lastError().text();
            return false;
        }
    }
    if (!db.commit()) {
        if (errMsg)
            *errMsg = db.lastError().text();
        return false;
    }
    return true;
}
