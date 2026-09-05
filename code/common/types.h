#ifndef TYPES_H
#define TYPES_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// 业务枚举(数值与数据库状态字段约定一致, 见 server/DatabaseManager.cpp 的 createTables)
enum PileType    { PileFast = 0, PileSlow = 1 };
enum PileStatus  { PileIdle = 0, PileInUse = 1, PileFault = 2 };
enum UserStatus  { UserNormal = 0, UserFrozen = 1 };

// 订单状态: 0/1 为原语义(向后兼容旧演示数据), 2~4 为新增
enum OrderStatus {
    OrderCharging  = 0,   // 充电中
    OrderFinished  = 1,   // 已完成
    OrderWaiting   = 2,   // (保留)排队中; 排队实体实际独立存于 charge_reservation
    OrderCancelled = 3,   // 已取消
    OrderAbnormal  = 4,   // 异常中断(桩故障等)
};

// 订单结束原因(charge_order.finish_type)
enum FinishType {
    FinishByUser    = 0,   // 用户手动结束
    FinishByTarget  = 1,   // 达到充电目标自动结束
    FinishByBalance = 2,   // 余额耗尽自动结束
    FinishByTimeout = 3,   // 超时(保留)
    FinishByAdmin   = 4,   // 管理员强制结束
    FinishByFault   = 5,   // 桩故障中断
};

// 充电目标类型(charge_order.target_type)
enum TargetType {
    TargetNone    = 0,   // 不限, 手动结束
    TargetEnergy  = 1,   // 按电量(度)
    TargetAmount  = 2,   // 按金额(元)
    TargetMinutes = 3,   // 按时长(分钟)
};

// 排队/预约类型(charge_reservation.type)
enum ReservationType {
    ReserveQueue   = 0,   // 现场排队
    ReserveAppoint = 1,   // 提前预约时段
};

// 排队/预约状态(charge_reservation.status)
enum ReservationStatus {
    ReservationActive   = 0,   // 有效(排队中 / 预约待履约)
    ReservationAssigned = 1,   // 已分配待确认(排队轮到)
    ReservationCanceled = 2,   // 已取消
    ReservationExpired  = 3,   // 已过期
    ReservationFulfilled= 4,   // 已履约(已转充电订单)
};

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
    // ---- v2 扩展 ----
    double freezeAmount = 0.0;   // 预授权冻结金额
    int targetType = TargetNone; // 充电目标类型
    double targetValue = 0.0;    // 目标数值
    double priceSnapshot = 0.0;  // 计费电价快照(元/度, 含服务费)
    int finishType = FinishByUser;
    QString cancelReason;
    double refundAmount = 0.0;
    int simMinutes = 0;          // 已模拟充电分钟数(引擎推进, 断线/重启可恢复)

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
        r.freezeAmount = o.value("freezeAmount").toDouble();
        r.targetType = o.value("targetType").toInt();
        r.targetValue = o.value("targetValue").toDouble();
        r.priceSnapshot = o.value("priceSnapshot").toDouble();
        r.finishType = o.value("finishType").toInt();
        r.cancelReason = o.value("cancelReason").toString();
        r.refundAmount = o.value("refundAmount").toDouble();
        r.simMinutes = o.value("simMinutes").toInt();
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
        o.insert("freezeAmount", freezeAmount);
        o.insert("targetType", targetType);
        o.insert("targetValue", targetValue);
        o.insert("priceSnapshot", priceSnapshot);
        o.insert("finishType", finishType);
        o.insert("cancelReason", cancelReason);
        o.insert("refundAmount", refundAmount);
        o.insert("simMinutes", simMinutes);
        return o;
    }
};

// 分时费率规则(price_rule)
struct FeeRule
{
    int id = 0;
    int stationId = 0;
    int period = 1;             // 0=低谷 1=平段 2=高峰
    QString startTime;          // HH:MM
    QString endTime;            // HH:MM
    double price = 0.0;         // 电价 元/度
    double serviceFee = 0.0;    // 服务费 元/度

    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("id", id);
        o.insert("stationId", stationId);
        o.insert("period", period);
        o.insert("startTime", startTime);
        o.insert("endTime", endTime);
        o.insert("price", price);
        o.insert("serviceFee", serviceFee);
        return o;
    }
};

// 排队/预约记录(charge_reservation; pileCode/stationName/phoneMasked 为联表附加)
struct ReservationInfo
{
    int id = 0;
    int userId = 0;
    int pileId = 0;
    int stationId = 0;
    int type = ReserveQueue;
    QString createTime;
    QString assignTime;
    QString expireTime;
    QString reserveDate;
    QString reserveStart;
    QString reserveEnd;
    int status = ReservationActive;
    QString pileCode;
    QString stationName;
    QString phoneMasked;
    int queuePos = 0;           // 排队位置(附加, 从1开始)

    static ReservationInfo fromJson(const QJsonObject &o)
    {
        ReservationInfo r;
        r.id = o.value("reservationId").toInt();
        r.userId = o.value("userId").toInt();
        r.pileId = o.value("pileId").toInt();
        r.stationId = o.value("stationId").toInt();
        r.type = o.value("type").toInt();
        r.createTime = o.value("createTime").toString();
        r.assignTime = o.value("assignTime").toString();
        r.expireTime = o.value("expireTime").toString();
        r.reserveDate = o.value("reserveDate").toString();
        r.reserveStart = o.value("reserveStart").toString();
        r.reserveEnd = o.value("reserveEnd").toString();
        r.status = o.value("status").toInt();
        r.pileCode = o.value("pileCode").toString();
        r.stationName = o.value("stationName").toString();
        r.phoneMasked = o.value("phoneMasked").toString();
        r.queuePos = o.value("queuePos").toInt();
        return r;
    }
    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("reservationId", id);
        o.insert("userId", userId);
        o.insert("pileId", pileId);
        o.insert("stationId", stationId);
        o.insert("type", type);
        o.insert("createTime", createTime);
        o.insert("assignTime", assignTime);
        o.insert("expireTime", expireTime);
        o.insert("reserveDate", reserveDate);
        o.insert("reserveStart", reserveStart);
        o.insert("reserveEnd", reserveEnd);
        o.insert("status", status);
        o.insert("pileCode", pileCode);
        o.insert("stationName", stationName);
        o.insert("phoneMasked", phoneMasked);
        o.insert("queuePos", queuePos);
        return o;
    }
};

#endif // TYPES_H
