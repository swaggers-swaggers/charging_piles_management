#ifndef GEOUTIL_H
#define GEOUTIL_H

#include <QtGlobal>
#include <QtMath>

// 地理计算工具(双端共用)
namespace GeoUtil {

inline bool outsideMainlandChina(double lon, double lat)
{
    return lon < 72.004 || lon > 137.8347 || lat < 0.8293 || lat > 55.8271;
}

inline double gcjTransformLatitude(double lonOffset, double latOffset)
{
    double value = -100.0 + 2.0 * lonOffset + 3.0 * latOffset
                   + 0.2 * latOffset * latOffset + 0.1 * lonOffset * latOffset
                   + 0.2 * qSqrt(qAbs(lonOffset));
    value += (20.0 * qSin(6.0 * lonOffset * M_PI)
              + 20.0 * qSin(2.0 * lonOffset * M_PI)) * 2.0 / 3.0;
    value += (20.0 * qSin(latOffset * M_PI)
              + 40.0 * qSin(latOffset / 3.0 * M_PI)) * 2.0 / 3.0;
    value += (160.0 * qSin(latOffset / 12.0 * M_PI)
              + 320.0 * qSin(latOffset * M_PI / 30.0)) * 2.0 / 3.0;
    return value;
}

inline double gcjTransformLongitude(double lonOffset, double latOffset)
{
    double value = 300.0 + lonOffset + 2.0 * latOffset
                   + 0.1 * lonOffset * lonOffset + 0.1 * lonOffset * latOffset
                   + 0.1 * qSqrt(qAbs(lonOffset));
    value += (20.0 * qSin(6.0 * lonOffset * M_PI)
              + 20.0 * qSin(2.0 * lonOffset * M_PI)) * 2.0 / 3.0;
    value += (20.0 * qSin(lonOffset * M_PI)
              + 40.0 * qSin(lonOffset / 3.0 * M_PI)) * 2.0 / 3.0;
    value += (150.0 * qSin(lonOffset / 12.0 * M_PI)
              + 300.0 * qSin(lonOffset / 30.0 * M_PI)) * 2.0 / 3.0;
    return value;
}

// 国内商业地图通常使用 GCJ-02，OpenStreetMap/OSRM 使用 WGS-84。
// 该转换只用于显示和路线预览；高德 URI 仍直接使用原始 GCJ-02 坐标。
inline void wgs84ToGcj02(double wgsLon, double wgsLat, double &gcjLon, double &gcjLat)
{
    if (outsideMainlandChina(wgsLon, wgsLat)) {
        gcjLon = wgsLon;
        gcjLat = wgsLat;
        return;
    }

    constexpr double a = 6378245.0;
    constexpr double ee = 0.00669342162296594323;
    double dLat = gcjTransformLatitude(wgsLon - 105.0, wgsLat - 35.0);
    double dLon = gcjTransformLongitude(wgsLon - 105.0, wgsLat - 35.0);
    const double radLat = qDegreesToRadians(wgsLat);
    const double magic = 1.0 - ee * qSin(radLat) * qSin(radLat);
    const double sqrtMagic = qSqrt(magic);
    dLat = (dLat * 180.0) / ((a * (1.0 - ee)) / (magic * sqrtMagic) * M_PI);
    dLon = (dLon * 180.0) / (a / sqrtMagic * qCos(radLat) * M_PI);
    gcjLat = wgsLat + dLat;
    gcjLon = wgsLon + dLon;
}

inline void gcj02ToWgs84(double gcjLon, double gcjLat, double &wgsLon, double &wgsLat)
{
    if (outsideMainlandChina(gcjLon, gcjLat)) {
        wgsLon = gcjLon;
        wgsLat = gcjLat;
        return;
    }

    // 迭代反解，避免一次近似在高缩放级别出现明显点位偏差。
    double minLon = gcjLon - 0.02;
    double maxLon = gcjLon + 0.02;
    double minLat = gcjLat - 0.02;
    double maxLat = gcjLat + 0.02;
    for (int i = 0; i < 30; ++i) {
        wgsLon = (minLon + maxLon) / 2.0;
        wgsLat = (minLat + maxLat) / 2.0;
        double testLon = 0.0;
        double testLat = 0.0;
        wgs84ToGcj02(wgsLon, wgsLat, testLon, testLat);
        if (testLon < gcjLon)
            minLon = wgsLon;
        else
            maxLon = wgsLon;
        if (testLat < gcjLat)
            minLat = wgsLat;
        else
            maxLat = wgsLat;
    }
}

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
