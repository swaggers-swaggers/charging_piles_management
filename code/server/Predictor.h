#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <QHash>
#include <QString>
#include <QVector>

// 充电负荷智能预测(简化版, 纯 C++ 无第三方依赖)
// 算法: 按小时聚合近 N 天已完成订单的充电量 → 计算每个"小时段"的历史平均负荷
//       → 乘以星期系数(工作日/周末差异) → 3 点滑动平均平滑 → 输出未来 24 小时预测
class Predictor
{
public:
    // 未来 24 小时的全网预测负荷(kWh), 索引 0 = 当前小时后的第 1 小时
    // connName: 数据库连接名, 工作线程(如 ClientHandler)调用时需传入线程私有连接
    static QVector<double> forecast24h(const QString &connName = QString());

    // 未来第 hourAhead(1~24) 小时的全网预测负荷(kWh)
    static double forecastAt(int hourAhead, const QString &connName = QString());

    // 某电站未来第 hourAhead 小时的预计空闲率(0~1):
    //   以当前空闲率为基准, 按预测小时负荷与全站总功率的比值衰减
    static double predictIdleRate(int stationId, int totalPiles, int idlePiles, int hourAhead,
                                  const QString &connName = QString());
};

#endif // PREDICTOR_H
