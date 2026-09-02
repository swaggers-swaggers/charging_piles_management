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
    ReqStartCharge      = 9,   // {userId, pileId} → {order:{...OrderInfo}}
                               //   失败: 桩非空闲(ErrPileBusy) / 已有未完成订单(ErrOrderExists)
    ReqStopCharge       = 10,  // {orderId, userId} → {order:{...OrderInfo}, balance}  结算并扣余额

    // 服务端推送
    PushOrderProgress   = 101, // {orderId, energy, amount, minutes} 充电进度(定时推送)
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
};

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
