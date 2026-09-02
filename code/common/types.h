#ifndef TYPES_H
#define TYPES_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// 业务枚举(数值与数据库状态字段约定一致, 见 server/DatabaseManager.cpp 的 createTables)
enum PileType    { PileFast = 0, PileSlow = 1 };
enum PileStatus  { PileIdle = 0, PileInUse = 1, PileFault = 2 };
enum UserStatus  { UserNormal = 0, UserFrozen = 1 };
enum OrderStatus { OrderCharging = 0, OrderFinished = 1 };

// 用户信息
struct UserInfo
{
    int id = 0;
    QString phone;
    QString nickname;
    QString avatar;
    double balance = 0.0;
    int status = UserNormal;
    QString registerTime;

    static UserInfo fromJson(const QJsonObject &o)
    {
        UserInfo u;
        u.id = o.value("id").toInt();
        u.phone = o.value("phone").toString();
        u.nickname = o.value("nickname").toString();
        u.avatar = o.value("avatar").toString();
        u.balance = o.value("balance").toDouble();
        u.status = o.value("status").toInt();
        u.registerTime = o.value("registerTime").toString();
        return u;
    }
    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("id", id);
        o.insert("phone", phone);
        o.insert("nickname", nickname);
        o.insert("avatar", avatar);
        o.insert("balance", balance);
        o.insert("status", status);
        o.insert("registerTime", registerTime);
        return o;
    }
};

// 充电站信息(totalPiles/idlePiles/distance/predictIdle 为列表查询的附加统计)
struct StationInfo
{
    int id = 0;
    QString name;
    QString address;
    double longitude = 0.0;
    double latitude = 0.0;
    double price = 0.0;          // 元/度
    QString createTime;
    int totalPiles = 0;
    int idlePiles = 0;
    double distance = -1.0;      // 公里, <0 表示未计算
    double predictIdle = -1.0;   // 预计空闲率 0~1, <0 表示无预测(阶段5)

    static StationInfo fromJson(const QJsonObject &o)
    {
        StationInfo s;
        s.id = o.value("id").toInt();
        s.name = o.value("name").toString();
        s.address = o.value("address").toString();
        s.longitude = o.value("longitude").toDouble();
        s.latitude = o.value("latitude").toDouble();
        s.price = o.value("price").toDouble();
        s.totalPiles = o.value("totalPiles").toInt();
        s.idlePiles = o.value("idlePiles").toInt();
        s.distance = o.value("distance").toDouble();
        s.predictIdle = o.value("predictIdle").toDouble();
        return s;
    }
    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("id", id);
        o.insert("name", name);
        o.insert("address", address);
        o.insert("longitude", longitude);
        o.insert("latitude", latitude);
        o.insert("price", price);
        o.insert("totalPiles", totalPiles);
        o.insert("idlePiles", idlePiles);
        o.insert("distance", distance);
        o.insert("predictIdle", predictIdle);
        return o;
    }
};

// 充电桩信息(stationName 为联表附加字段)
struct PileInfo
{
    int id = 0;
    int stationId = 0;
    QString code;
    int type = PileFast;
    double power = 0.0;          // kW
    int status = PileIdle;
    int totalCount = 0;          // 累计充电次数
    int totalDuration = 0;       // 累计充电时长(分钟)
    QString stationName;

    static PileInfo fromJson(const QJsonObject &o)
    {
        PileInfo p;
        p.id = o.value("id").toInt();
        p.stationId = o.value("stationId").toInt();
        p.code = o.value("code").toString();
        p.type = o.value("type").toInt();
        p.power = o.value("power").toDouble();
        p.status = o.value("status").toInt();
        p.totalCount = o.value("totalCount").toInt();
        p.totalDuration = o.value("totalDuration").toInt();
        p.stationName = o.value("stationName").toString();
        return p;
    }
    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("id", id);
        o.insert("stationId", stationId);
        o.insert("code", code);
        o.insert("type", type);
        o.insert("power", power);
        o.insert("status", status);
        o.insert("totalCount", totalCount);
        o.insert("totalDuration", totalDuration);
        o.insert("stationName", stationName);
        return o;
    }
};

// 充电订单信息(pileCode/stationName 为联表附加字段)
struct OrderInfo
{
    int id = 0;
    int userId = 0;
    int pileId = 0;
    int stationId = 0;
    QString startTime;
    QString endTime;
    double energy = 0.0;         // 度
    double amount = 0.0;         // 元
    int status = OrderCharging;
    QString pileCode;
    QString stationName;

    static OrderInfo fromJson(const QJsonObject &o)
    {
        OrderInfo r;
        r.id = o.value("orderId").toInt();
        r.userId = o.value("userId").toInt();
        r.pileId = o.value("pileId").toInt();
        r.stationId = o.value("stationId").toInt();
        r.startTime = o.value("startTime").toString();
        r.endTime = o.value("endTime").toString();
        r.energy = o.value("energy").toDouble();
        r.amount = o.value("amount").toDouble();
        r.status = o.value("status").toInt();
        r.pileCode = o.value("pileCode").toString();
        r.stationName = o.value("stationName").toString();
        return r;
    }
    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("orderId", id);
        o.insert("userId", userId);
        o.insert("pileId", pileId);
        o.insert("stationId", stationId);
        o.insert("startTime", startTime);
        o.insert("endTime", endTime);
        o.insert("energy", energy);
        o.insert("amount", amount);
        o.insert("status", status);
        o.insert("pileCode", pileCode);
        o.insert("stationName", stationName);
        return o;
    }
};

#endif // TYPES_H
