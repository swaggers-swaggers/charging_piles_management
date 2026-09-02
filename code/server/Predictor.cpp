#include "Predictor.h"

#include "PileDao.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtMath>

namespace {
// 平滑: 3 点滑动平均
QVector<double> smooth(const QVector<double> &input)
{
    const int n = input.size();
    QVector<double> out(n);
    for (int i = 0; i < n; ++i) {
        double sum = input[i];
        int cnt = 1;
        if (i > 0)     { sum += input[i - 1]; ++cnt; }
        if (i + 1 < n) { sum += input[i + 1]; ++cnt; }
        out[i] = sum / cnt;
    }
    return out;
}
} // namespace

QVector<double> Predictor::forecast24h(const QString &connName)
{
    // 1. 历史负荷: 聚合近 14 天已完成订单, 按"一天中的小时"累加充电量
    QVector<double> hourlySum(24, 0.0);
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("SELECT strftime('%H', start_time) AS h, SUM(energy)"
                  " FROM charge_order"
                  " WHERE status = 1 AND start_time >= datetime('now','localtime','-14 days')"
                  " GROUP BY h");
    if (query.exec()) {
        while (query.next()) {
            const int hour = query.value(0).toString().toInt();
            if (hour >= 0 && hour < 24)
                hourlySum[hour] += query.value(1).toDouble();
        }
    }

    // 2. 平均: 每小时段的日平均负荷(kWh)
    QVector<double> hourlyAvg(24, 0.0);
    for (int h = 0; h < 24; ++h)
        hourlyAvg[h] = hourlySum[h] / 14.0;

    // 3. 星期系数: 周末出行充电略高, 工作日通勤早晚高峰略高
    QVector<double> hourAhead(24);
    const QDateTime base = QDateTime::currentDateTime();
    for (int i = 0; i < 24; ++i) {
        const QDateTime t = base.addSecs((i + 1) * 3600);
        const int dow = t.date().dayOfWeek();          // 1=周一 ... 7=周日
        double factor = 1.0;
        if (dow >= 6)
            factor = 1.15;                              // 周末整体 +15%
        const int hour = t.time().hour();
        if (hour >= 7 && hour <= 9)      factor *= 1.10;   // 早高峰
        else if (hour >= 17 && hour <= 20) factor *= 1.20; // 晚高峰
        else if (hour >= 0 && hour <= 5)  factor *= 0.50;  // 深夜低谷
        hourAhead[i] = hourlyAvg[hour] * factor;
    }

    // 4. 环形平滑(首尾相接, 保证曲线连续)
    QVector<double> ring = hourAhead;
    ring.append(hourAhead.first());
    QVector<double> smoothed = smooth(ring);
    smoothed.removeLast();

    // 数据不足时(新库无订单)给出一个典型日内曲线, 便于演示
    bool allZero = true;
    for (double v : smoothed) {
        if (v > 0.01) { allZero = false; break; }
    }
    if (allZero) {
        QVector<double> typical(24);
        for (int h = 0; h < 24; ++h) {
            double v = 8.0;
            if (h >= 7 && h <= 9)        v = 22.0;
            else if (h >= 17 && h <= 20) v = 30.0;
            else if (h >= 0 && h <= 5)   v = 2.0;
            else if (h >= 10 && h <= 16) v = 14.0;
            typical[h] = v;
        }
        return typical;
    }
    return smoothed;
}

double Predictor::forecastAt(int hourAhead, const QString &connName)
{
    const QVector<double> f = forecast24h(connName);
    if (f.isEmpty())
        return 0.0;
    return f[qBound(1, hourAhead, 24) - 1];
}

double Predictor::predictIdleRate(int stationId, int totalPiles, int idlePiles, int hourAhead,
                                  const QString &connName)
{
    Q_UNUSED(stationId);
    if (totalPiles <= 0)
        return 0.0;

    // 该站总功率(kW) → 1 小时内可输出的电量(kWh)
    double stationPowerKw = 0.0;
    const QList<PileInfo> piles = PileDao::listByStation(stationId, connName);
    for (const PileInfo &p : piles) {
        if (p.status != PileFault)
            stationPowerKw += p.power;
    }
    if (stationPowerKw <= 0)
        return 0.0;

    // 全网未来一小时预测负荷按各站功率占比分摊到本站
    const double netLoad = forecastAt(hourAhead, connName);
    double totalPower = 0.0;
    const QList<PileInfo> all = PileDao::listAll(connName);
    for (const PileInfo &p : all) {
        if (p.status != PileFault)
            totalPower += p.power;
    }
    const double stationLoad = (totalPower > 0) ? netLoad * stationPowerKw / totalPower : 0.0;

    // 预计占用桩数 = 负荷电量 / (平均单桩功率 * 1 小时), 不超过总桩数
    const double avgPower = stationPowerKw / totalPiles;
    const double busyPiles = qMin<double>(totalPiles, stationLoad / qMax(avgPower, 0.1));
    const double idleRate = (totalPiles - busyPiles) / totalPiles;

    // 与当前空闲率加权融合, 避免预测与现状脱节
    const double currentRate = static_cast<double>(idlePiles) / totalPiles;
    return qBound(0.0, 0.6 * idleRate + 0.4 * currentRate, 1.0);
}
