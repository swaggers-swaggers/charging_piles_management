#ifndef CHARGINGENGINE_H
#define CHARGINGENGINE_H

#include "types.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>

class QTimer;
class ClientHandler;

// 全局充电引擎(单例, 运行在主线程)
//
// 职责:
//  1. 统一推进所有充电中订单(3 真实秒 = 1 模拟分钟), 累计电量/金额并推送进度;
//     订单状态全部落库, 客户端断线、服务端重启都不影响充电继续/恢复。
//  2. 统一结算 settleOrder(): 一个事务内解冻预授权、实扣、落单、释放桩并分配队首。
//  3. 排队: 桩释放后自动分配最早排队者(30 秒未确认自动过期, 顺延下一位)。
//  4. 预约: 开始前 10 分钟提醒、结束宽限期后未到桩自动过期。
//  5. startCharging(): 原子抢桩 + 余额冻结 + 建单的开启事务。
class ChargingEngine : public QObject
{
    Q_OBJECT

public:
    static ChargingEngine &instance();

    // 数据库初始化完成后调用: 恢复在充订单、释放孤儿桩、启动心跳
    void start();

    // ---------- 在线连接注册表(跨线程, 线程安全) ----------
    void registerClient(int userId, ClientHandler *handler);
    void unregisterClient(ClientHandler *handler);
    // 给指定用户推送一条消息(不在线则丢弃; 自动跨线程排队到连接线程发送)
    void pushToUser(int userId, const QJsonObject &msg);

    // ---------- 开启充电(原子抢桩 + 冻结 + 建单, 一个事务) ----------
    struct StartResult {
        bool ok = false;
        int errorCode = 0;          // Protocol::ErrorCode
        QString error;
        OrderInfo order;
        double freezeAmount = 0.0;
        double unitPrice = 0.0;     // 计费单价快照
        double balanceAfter = 0.0;
    };
    static StartResult startCharging(int userId, int pileId, int targetType,
                                     double targetValue, const QString &connName);

    // ---------- 统一结算(解冻 + 实扣 + 落单 + 释放桩 + 分配队首) ----------
    struct SettleResult {
        bool ok = false;
        QString error;
        OrderInfo order;
        double balanceAfter = 0.0;
    };
    SettleResult settleOrder(int orderId, int finishType, const QString &reason,
                             const QString &connName = QString());
    // 管理端强制结束(默认连接, 并向用户推送结束事件)
    SettleResult forceFinish(int orderId, const QString &reason);
    // 管理端故障退款: 把订单实扣金额(或指定额)退回钱包并记录 refund_amount
    static bool refundOrder(int orderId, double amount, QString *err,
                            const QString &connName = QString());

    // 桩释放后把桩分配给最早排队者(置已分配待确认 + 推送"轮到你了")
    void assignQueueHead(int pileId, const QString &connName = QString());

    // 预授权冻结额计算; err 非空且返回值<0 表示参数/余额不合法, errorCode 给出错误码
    static double calcFreeze(int targetType, double targetValue,
                             double power, double unitPrice, double balance,
                             int *errorCode = nullptr);

private slots:
    void onTick();

private:
    explicit ChargingEngine(QObject *parent = nullptr);

    void recoverOnStart();
    void sweepActiveOrders();
    void sweepReservations();
    void notifyOrderEnded(const OrderInfo &order, int finishType, const QString &reason);

    QTimer *m_timer = nullptr;
    bool m_started = false;
    QHash<int, ClientHandler *> m_online;   // userId -> 连接处理器
    QMutex m_onlineMutex;
};

#endif // CHARGINGENGINE_H
