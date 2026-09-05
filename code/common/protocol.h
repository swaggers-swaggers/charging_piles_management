#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

// 通信协议: TCP 长连接, UTF-8 JSON, 每条消息一行(以 '\n' 结尾, 内容为单行 Compact JSON)
//
// 请求:  客户端 → 服务端   {"type": N, ...参数}
// 应答:  服务端 → 客户端   {"type": N, "ok": true/false, "error": "...", ...结果字段}
// 推送:  服务端 → 客户端   {"type": N >= 100, ...数据}
//
// 本文件即协议文档: 每个消息类型注释了请求参数与应答/推送字段。
namespace Protocol {

// ---- 消息类型 ----
enum MessageType {
    // 客户端请求 (应答回显相同 type)
    ReqHeartbeat        = 1,   // {} → {ok}
    ReqUserLogin        = 2,   // {phone} → {userId, phone, nickname, balance, isNew}
    ReqGetUserInfo      = 3,   // {userId} → {nickname, balance, avatar, status}
    ReqUpdateProfile    = 4,   // {userId, nickname?, avatar?} → {ok, nickname, balance, avatar}
                               //   只更新给出的字段; avatar 为 96x96 PNG 图片的 base64 编码
    ReqRecharge         = 5,   // {userId, amount} → {balance}
    ReqStationList      = 6,   // {lon, lat} → {stations:[{id,name,address,price,totalPiles,
                               //              idlePiles,distance,predictIdle}]} 按距离升序
    ReqStationPiles     = 7,   // {stationId} → {piles:[{id,code,type,power,status}]}
    ReqUnfinishedOrder  = 8,   // {userId} → {hasOrder, order:{...OrderInfo}}
    ReqStartCharge      = 9,   // {userId, pileId} → {order:{...OrderInfo}}  (兼容旧客户端: 等同不限目标)
                               //   失败: 桩非空闲(ErrPileBusy) / 已有未完成订单(ErrOrderExists)
    ReqStopCharge       = 10,  // {orderId, userId} → {order:{...OrderInfo}, balance}  结算并扣余额

    // ---- v2 新增 ----
    ReqStartChargeExt   = 11,  // {userId, pileId, targetType?, targetValue?}
                               //   → {order:{...OrderInfo}, freezeAmount, price}
                               //   失败: ErrFreezeNotEnough / ErrPileBusy / ErrOrderExists / ErrTargetInvalid
    ReqReservePile      = 12,  // 现场排队: {userId, pileId, action:0排队/1取消, reservationId?}
                               //   排队 → {reservationId, queuePos}; 取消 → {ok}
    ReqOrderHistory     = 13,  // {userId, page?, pageSize?} → {orders:[...OrderInfo], total}
    ReqOrderDetail      = 14,  // {userId, orderId} → {order:{...OrderInfo}}
    ReqStationFee       = 15,  // {stationId} → {rules:[...FeeRule], defaultPrice}
    ReqAppointPile      = 16,  // 预约: {userId, pileId, reserveDate, reserveStart, reserveEnd}
                               //   → {reservationId}; 失败: ErrSlotConflict / ErrSlotPast
    ReqMyReservations   = 17,  // {userId} → {reservations:[...ReservationInfo]}
    ReqAppointSlots     = 18,  // {pileId, date} → {slots:["08:00",...], booked:[{start,end}]}

    // 服务端推送
    PushOrderProgress   = 101, // {orderId, energy, amount, minutes, targetType?, targetValue?, targetProgress?}
    PushOrderEvent      = 102, // {orderId?, reservationId?, event, message, queuePos?}
                               //   event: 1=排队轮到(请确认开始) 2=订单已结束(自动/管理员)
                               //          3=订单异常中断 4=排队位置变化
                               //          5=充电已开始 6=预约开始提醒 7=预约成功/已取消
};

// ---- 错误码(error 字段文本直接可展示, code 用于程序判断) ----
enum ErrorCode {
    ErrNone        = 0,
    ErrBadRequest  = 1,   // JSON 不合法 / 缺少参数
    ErrUnknownType = 2,
    ErrNotFound    = 3,   // 用户 / 电桩 / 订单不存在
    ErrPileBusy    = 4,
    ErrOrderExists = 5,
    ErrUserFrozen  = 6,
    ErrDbError     = 7,
    ErrInternal    = 8,
    ErrBalanceNotEnough = 9,   // 余额不足, 结算被拒
    // ---- v2 新增 ----
    ErrFreezeNotEnough = 10,   // 余额不足, 预授权冻结失败
    ErrQueueExists     = 11,   // 已有排队/预约记录
    ErrQueueFull       = 12,   // 排队人数上限
    ErrOrderNotActive  = 13,   // 订单不在可操作状态
    ErrTargetInvalid   = 14,   // 充电目标参数非法
    ErrStationNoFee    = 15,   // 站点无费率配置
    ErrSlotConflict    = 16,   // 预约时段冲突
    ErrSlotPast        = 17,   // 预约时段已过
};

// ---- 充电业务常量(双端共享, 与 ChargingEngine 保持一致) ----
namespace ChargeConfig {
    constexpr int    kTickMs          = 3000;  // 引擎心跳: 3 秒真实时间
    constexpr int    kMinutesPerTick  = 1;     // 每心跳 = 1 模拟分钟
    constexpr double kDefaultFreeze   = 50.0;  // 不限目标时的默认冻结额(元)
    constexpr double kMinFreeze       = 10.0;  // 最低可用余额, 低于则拒绝开始
    constexpr int    kQueueConfirmSec = 30;    // 排队轮到后的确认时限(真实秒)
    constexpr int    kAppointRemindMin= 10;    // 预约开始前提醒分钟数
    constexpr int    kAppointGraceMin = 15;    // 预约结束后宽限分钟数, 超时未到桩则过期
    constexpr int    kQueueMax        = 10;    // 单桩排队人数上限
}

// ---- 服务器地址 ----
// 默认同机回环测试; 可用环境变量 CHARGING_SERVER_HOST / CHARGING_SERVER_PORT 覆盖
inline QString serverHost()
{
    return qEnvironmentVariable("CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1"));
}

inline int serverPort()
{
    const int p = qEnvironmentVariableIntValue("CHARGING_SERVER_PORT");
    return p > 0 ? p : 9527;
}

// ---- 构造应答的小工具 ----
inline QJsonObject makeReply(int type, bool ok, const QString &error = QString())
{
    QJsonObject o;
    o.insert("type", type);
    o.insert("ok", ok);
    if (!error.isEmpty())
        o.insert("error", error);
    return o;
}

} // namespace Protocol

#endif // PROTOCOL_H
