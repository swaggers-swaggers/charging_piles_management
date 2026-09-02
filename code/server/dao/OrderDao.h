#ifndef ORDERDAO_H
#define ORDERDAO_H

#include "types.h"

#include <QList>
#include <QString>
#include <QVector>

// 充电订单数据访问
struct OrderContext
{
    OrderInfo order;
    double power = 0.0;   // 桩功率 kW
    double price = 0.0;   // 电价 元/度
};

class OrderDao
{
public:
    static bool create(int userId, int pileId, int stationId, const QString &startTime,
                       int *orderId = nullptr, QString *errMsg = nullptr,
                       const QString &connName = QString());

    static bool getById(int orderId, OrderInfo *out, QString *errMsg = nullptr,
                        const QString &connName = QString());

    // 用户当前"充电中"的订单
    static bool getUnfinishedByUser(int userId, OrderInfo *out, bool *has,
                                    QString *errMsg = nullptr, const QString &connName = QString());

    // 订单详情 + 桩功率 + 电价(充电模拟计算用)
    static bool getContext(int orderId, OrderContext *out, QString *errMsg = nullptr,
                           const QString &connName = QString());

    static bool updateProgress(int orderId, double energy, double amount,
                               const QString &connName = QString());

    // 完成订单(写入结束时间/电量/金额, 状态置已完成)
    static bool finish(int orderId, const QString &endTime, double energy, double amount,
                       QString *errMsg = nullptr, const QString &connName = QString());

    // ---- 销售统计(只统计已完成订单) ----
    static bool salesSummary(double *today, double *month, double *total,
                             QString *errMsg = nullptr, const QString &connName = QString());

    // 近 lastDays 天每日营收(按日期升序, 缺失日期补 0)
    static QVector<QPair<QString, double>> dailyRevenue(int lastDays,
                                                        QString *errMsg = nullptr,
                                                        const QString &connName = QString());

    // 各站累计营收(降序)
    static QList<QPair<QString, double>> stationRevenue(QString *errMsg = nullptr,
                                                        const QString &connName = QString());
};

#endif // ORDERDAO_H
