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
    if (!inOut || inOut->name.trimmed().isEmpty() || inOut->address.trimmed().isEmpty()) {
        if (errMsg)
            *errMsg = "站名和详细地址不能为空";
        return false;
    }
    if (pileCount <= 0 || pileCount > 50 || inOut->longitude < 0 || inOut->longitude > 180
        || inOut->latitude < 0 || inOut->latitude > 90 || inOut->price <= 0
        || inOut->price > 5) {
        if (errMsg)
            *errMsg = "新增参数不合法，请检查坐标、电价和电桩数量";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(connName);
    if (!db.isOpen()) {
        if (errMsg)
            *errMsg = "数据库未连接";
        return false;
    }
    if (!db.transaction()) {
        if (errMsg)
            *errMsg = "无法开启新增事务: " + db.lastError().text();
        return false;
    }
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM station WHERE name = :n");
    query.bindValue(":n", inOut->name.trimmed());
    if (!query.exec() || (query.next() && query.value(0).toInt() > 0)) {
        if (errMsg)
            *errMsg = query.lastError().isValid() ? "检查站点名称失败: " + query.lastError().text()
                                                   : "站点名称已存在";
        db.rollback();
        return false;
    }

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

    // 电桩编号顺延: 按现有编号最大值递增，避免删除记录后发生重复编号。
    int seq = 0;
    QSqlQuery q2(db);
    if (!q2.exec("SELECT COALESCE(MAX(CAST(SUBSTR(code, 4) AS INTEGER)), 0) FROM pile")
        || !q2.next()) {
        if (errMsg)
            *errMsg = "读取电桩编号失败: " + q2.lastError().text();
        db.rollback();
        return false;
    }
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
