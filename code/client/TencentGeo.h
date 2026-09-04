#ifndef TENCENTGEO_H
#define TENCENTGEO_H

#include <QList>
#include <QPair>
#include <QString>

// 腾讯地图 WebService API 地理工具
// 地址 -> 经纬度(GCJ-02 火星坐标, 与腾讯地图一致)
namespace TencentGeo {

// 输入中文地址, 输出经纬度; 成功返回 true 并写入 lon/lat
// 网络失败 / key 无效 / 地址无法解析时返回 false, 并把原因写入 error(可展示)
bool geocode(const QString &address, double &lon, double &lat, QString &error);

// 基于本机出口 IP 获取当前位置(桌面程序无 GPS, 这是可用的"实时位置"近似方案)
// 精度为城市级(几公里~十几公里), 课程演示足够; 网络失败时返回 false 并写入 error
bool ipLocation(double &lon, double &lat, QString &error);

// 路线规划结果
struct RouteInfo {
    double distanceMeters = 0.0;   // 真实距离(米)
    double durationMinutes = 0.0;  // 真实预计时长(分钟)
    // 真实道路折线, 每项为 (纬度, 经度), 已从腾讯压缩格式解码
    QList<QPair<double, double>> polyline;
};

// 驾车/步行路线规划(起点 -> 终点), 成功返回 true 并写入 route
// driving 为 true 走驾车, false 走步行; 失败时返回 false 并写入 error
bool routePlan(bool driving,
               double fromLon, double fromLat,
               double toLon, double toLat,
               RouteInfo &route, QString &error);

} // namespace TencentGeo

#endif // TENCENTGEO_H
