#include "StationDao.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

QList<StationInfo> StationDao::list(const QString &connName)
{
    QList<StationInfo> stations;
    QSqlQuery query(QSqlDatabase::database(connName));
    const QString sql =
        "SELECT s.id, s.name, s.address, s.longitude, s.latitude, s.price, s.create_time,"
        " (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id) AS total,"
        " (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id AND p.status = 0) AS idle,"
        " (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id AND p.status = 2) AS fault"
        " FROM station s ORDER BY s.id";
    if (!query.exec(sql))
        return stations;

    while (query.next()) {
        StationInfo s;
        s.id = query.value(0).toInt();
        s.name = query.value(1).toString();
        s.address = query.value(2).toString();
        s.longitude = query.value(3).toDouble();
        s.latitude = query.value(4).toDouble();
        s.price = query.value(5).toDouble();
        s.createTime = query.value(6).toString();
        s.totalPiles = query.value(7).toInt();
        s.idlePiles = query.value(8).toInt();
        // 在线率 = (总桩数 - 故障数) / 总桩数, 由调用方(管理页面)计算展示
        stations.append(s);
    }
    return stations;
}

bool StationDao::add(StationInfo *inOut, int pileCount, QString *errMsg, const QString &connName)
{
    QSqlDatabase db = QSqlDatabase::database(connName);
    if (!db.transaction()) {
        // 事务开启失败不致命, 继续按无事务执行
    }
    QSqlQuery query(db);

    query.prepare("INSERT INTO station (name, address, longitude, latitude, price) "
                  "VALUES (:n, :a, :lon, :lat, :p)");
    query.bindValue(":n", inOut->name);
    query.bindValue(":a", inOut->address);
    query.bindValue(":lon", inOut->longitude);
    query.bindValue(":lat", inOut->latitude);
    query.bindValue(":p", inOut->price);
    if (!query.exec()) {
        *errMsg = "新增充电站失败: " + query.lastError().text();
        db.rollback();
        return false;
    }
    const int stationId = query.lastInsertId().toInt();

    // 电桩编号顺延: 取当前最大编号序号
    int seq = 0;
    QSqlQuery q2(db);
    if (q2.exec("SELECT COUNT(*) FROM pile") && q2.next())
        seq = q2.value(0).toInt();

    for (int i = 0; i < pileCount; ++i) {
        ++seq;
        const int type = (i % 2 == 0) ? PileFast : PileSlow;
        const double power = (type == PileFast) ? 60.0 : 7.0;
        QSqlQuery q3(db);
        q3.prepare("INSERT INTO pile (station_id, code, type, power, status) "
                   "VALUES (:sid, :code, :t, :pw, 0)");
        q3.bindValue(":sid", stationId);
        q3.bindValue(":code", QString("CP-%1").arg(seq, 3, 10, QLatin1Char('0')));
        q3.bindValue(":t", type);
        q3.bindValue(":pw", power);
        if (!q3.exec()) {
            *errMsg = "生成电桩失败: " + q3.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        *errMsg = "提交事务失败: " + db.lastError().text();
        db.rollback();
        return false;
    }
    inOut->id = stationId;
    inOut->totalPiles = pileCount;
    inOut->idlePiles = pileCount;
    return true;
}
