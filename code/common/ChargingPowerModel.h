#ifndef CHARGINGPOWERMODEL_H
#define CHARGINGPOWERMODEL_H
#include <algorithm>
#include <cmath>

// 课程演示模型，非车辆遥测：假定 60kWh 电池、初始 SOC 20%。
// 时间与订单 ID 决定连续波动，重启后可从数据库进度复算，无随机跳变。
namespace ChargingPowerModel {
inline double averageKw(double ratedKw, int orderId, double minute, double deliveredKwh)
{
    if (!std::isfinite(ratedKw) || ratedKw <= 0) return 0;
    const double t = std::max(0.0, minute);
    const double phase = (orderId % 97) * 0.37;
    const double ramp = 0.40 + 0.60 * std::min(1.0, t / 3.0);
    const double ripple = 0.90 + 0.065 * std::sin(t * 0.72 + phase)
                               + 0.025 * std::sin(t * 1.73 + phase * 0.6);
    const double soc = 0.20 + std::max(0.0, deliveredKwh) / 60.0;
    const double taper = soc <= 0.80 ? 1.0 : std::max(0.18, 1.0 - (soc - 0.80) * 4.1);
    return ratedKw * std::clamp(ramp * ripple * taper, 0.0, 1.0);
}
}
#endif
