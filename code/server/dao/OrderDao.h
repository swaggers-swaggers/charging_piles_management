#ifndef ORDERDAO_H
#define ORDERDAO_H

#include "types.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QVector>

// 充电订单 DAO
// errMsg/connName 约定同其他 DAO: connName 非空时使用线程私有连接
class OrderDao
{
public:
    // 创建订单(status=0 充电中), 返回订单 id(<=0 失败)
    // priceSnapshot: 开始时锁定的计费单价(元/度, 含服务费)
    // freezeAmount:  预授权冻结金额; targetType/Value: 充电目标
    static int create(int userId, int pileId, int stationId,
                      double priceSnapshot, double freezeAmount,
                      int targetType, double targetValue,
                      QString *errMsg = nullptr, const QString &connName = QString());

    static OrderInfo getById(int id, QString *errMsg = nullptr,
                             const QString &connName = QString());
    // 用户未完成订单(status=0 充电中), hasOrder 置位
    static OrderInfo getUnfinishedByUser(int userId, bool *hasOrder = nullptr,
                                         QString *errMsg = nullptr,
                                         const QString &connName = QString());

    // 充电推进所需的聚合上下文(订单 + 桩功率/状态 + 站点电价 + 用户余额)
    struct OrderContext {
        int orderId = 0;
        int userId = 0;
        int pileId = 0;
        int pileStatus = PileIdle;
        double power = 0.0;
        double price = 0.0;
        double energy = 0.0;
        double amount = 0.0;
        int simMinutes = 0;
        double freezeAmount = 0.0;
        int targetType = TargetNone;
        double targetValue = 0.0;
        double priceSnapshot = 0.0;
        double userBalance = 0.0;
        bool exists = false;
    };
    static OrderContext getContext(int orderId, QString *errMsg = nullptr,
                                   const QString &connName = QString());

    // 引擎每 tick 更新累计值
    static bool updateProgress(int orderId, double energy, double amount, int simMinutes,
                               QString *errMsg = nullptr, const QString &connName = QString());

    // 按结束原因落单(status=1 已完成), 写入结束时间/电量/金额/模拟分钟
    static bool finishWithType(int orderId, double energy, double amount, int simMinutes,
                               int finishType, const QString &reason = QString(),
                               QString *errMsg = nullptr, const QString &connName = QString());

    // 记录退款金额(累加, 管理端故障退款)
    static bool addRefund(int orderId, double amount,
                          QString *errMsg = nullptr, const QString &connName = QString());

    // 引擎: 全部充电中订单
    static QList<OrderInfo> listActive(QString *errMsg = nullptr,
                                       const QString &connName = QString());

    // 用户端: 订单历史(分页, 按时间倒序); total 返回总条数
    static QList<OrderInfo> listByUser(int userId, int page, int pageSize, int *total,
                                       QString *errMsg = nullptr,
                                       const QString &connName = QString());

    // 管理端: 全部订单; statusFilter=-1 表示不限状态
    static QList<OrderInfo> listAll(int statusFilter,
                                    QString *errMsg = nullptr,
                                    const QString &connName = QString());

    // ---- 销售业绩统计(只统计 status=1 已完成; 兼容 SalesPage/DataExporter) ----
    // today=今日营收, month=近30天营收, total=累计营收, 返回 false 表示查询出错
    static bool salesSummary(double *today, double *month, double *total,
                             QString *errMsg = nullptr,
                             const QString &connName = QString());
    // 近 N 日营收: (日期, 金额)
    static QVector<QPair<QString, double>> dailyRevenue(int days = 7,
                                                        QString *errMsg = nullptr,
                                                        const QString &connName = QString());
    // 各站营收: (站名, 金额), 按金额倒序
    static QList<QPair<QString, double>> stationRevenue(QString *errMsg = nullptr,
                                                        const QString &connName = QString());
};

#endif // ORDERDAO_H
