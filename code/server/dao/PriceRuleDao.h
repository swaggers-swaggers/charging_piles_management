#ifndef PRICERULEDAO_H
#define PRICERULEDAO_H

#include "types.h"

#include <QList>
#include <QString>
#include <QTime>

// 分时费率 DAO(price_rule)
class PriceRuleDao
{
public:
    // 站点的全部费率段(按 start_time 排序)
    static QList<FeeRule> listByStation(int stationId, QString *errMsg = nullptr,
                                        const QString &connName = QString());

    // 计算指定时刻的计费单价(电价 + 服务费, 元/度)
    // 无任何费率规则时返回 false(调用方回退站点基准价或报 ErrStationNoFee)
    static bool currentPrice(int stationId, double *totalPrice,
                             double *serviceFee = nullptr,
                             const QTime &when = QTime(),
                             QString *errMsg = nullptr,
                             const QString &connName = QString());

    // 管理端: 保存(覆盖)站点费率, 事务内先删后插
    static bool replaceForStation(int stationId, const QList<FeeRule> &rules,
                                  QString *errMsg = nullptr,
                                  const QString &connName = QString());

    // "HH:MM" → 分钟数; "24:00" 视为 1440
    static int hhmmToMinutes(const QString &hhmm);
};

#endif // PRICERULEDAO_H
