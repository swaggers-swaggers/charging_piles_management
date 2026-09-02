#ifndef GEOUTIL_H
#define GEOUTIL_H

#include <QtGlobal>
#include <QtMath>

// 地理计算工具(双端共用)
namespace GeoUtil {

// Haversine 公式: 两经纬度点之间的球面距离(公里)
inline double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371.0;
    const double dLat = qDegreesToRadians(lat2 - lat1);
    const double dLon = qDegreesToRadians(lon2 - lon1);
    const double a = qSin(dLat / 2) * qSin(dLat / 2)
                     + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
                       * qSin(dLon / 2) * qSin(dLon / 2);
    const double c = 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
    return R * c;
}

} // namespace GeoUtil

#endif // GEOUTIL_H
