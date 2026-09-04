#include "TencentGeo.h"

#include "mapconfig.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace TencentGeo {

namespace {

// 发起一次 HTTPS GET, 返回解析后的 JSON 根对象; 失败时把原因写入 error 并返回空对象
QJsonObject httpGetJson(const QUrl &url, QString &error)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("ChargingClient/1.0 (Qt)"));

    QNetworkAccessManager nam;
    QNetworkReply *reply = nam.get(req);

    // 同步等待(局部事件循环), 与项目现有 TcpClient::request() 风格一致
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(8000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        error = QStringLiteral("请求腾讯地图超时, 请检查网络");
        return QJsonObject();
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (httpStatus != 200) {
        error = QStringLiteral("腾讯地图返回 HTTP %1").arg(httpStatus);
        return QJsonObject();
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QStringLiteral("腾讯地图返回数据无法解析");
        return QJsonObject();
    }
    return doc.object();
}

QUrl buildUrl(const QString &path, const QUrlQuery &query)
{
    QUrl url(QStringLiteral("https://apis.map.qq.com") + path);
    url.setQuery(query);
    return url;
}

QString checkKey()
{
    const QString key = QString::fromLatin1(TENCENT_MAP_KEY);
    if (key.isEmpty() || key == QLatin1String("YOUR_KEY"))
        return QString();
    return key;
}

} // namespace

// 腾讯位置服务 地理编码接口
// 文档: https://lbs.qq.com/service/webService/webServiceGuide/webServiceGeocoder
bool geocode(const QString &address, double &lon, double &lat, QString &error)
{
    const QString key = checkKey();
    if (key.isEmpty()) {
        error = QStringLiteral("未配置腾讯地图 Key, 请编辑 code/client/mapconfig.h");
        return false;
    }
    if (address.trimmed().isEmpty()) {
        error = QStringLiteral("地址为空");
        return false;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    query.addQueryItem(QStringLiteral("key"), key);
    const QJsonObject root = httpGetJson(buildUrl(QStringLiteral("/ws/geocoder/v1/"), query), error);
    if (root.isEmpty())
        return false;

    const int status = root.value(QStringLiteral("status")).toInt();
    if (status != 0) {
        error = root.value(QStringLiteral("message"))
                    .toString(QStringLiteral("解析失败(状态码 %1)").arg(status));
        return false;
    }

    const QJsonObject location = root.value(QStringLiteral("result")).toObject()
                                     .value(QStringLiteral("location")).toObject();
    const double lng = location.value(QStringLiteral("lng")).toDouble();
    const double lat2 = location.value(QStringLiteral("lat")).toDouble();
    if (lng == 0.0 && lat2 == 0.0) {
        error = QStringLiteral("腾讯地图未返回有效坐标");
        return false;
    }

    lon = lng;
    lat = lat2;
    return true;
}

// 腾讯位置服务 IP 定位接口(不传 ip 参数则按请求来源 IP 定位)
// 文档: https://lbs.qq.com/service/webService/webServiceGuide/webServiceIp
bool ipLocation(double &lon, double &lat, QString &error)
{
    const QString key = checkKey();
    if (key.isEmpty()) {
        error = QStringLiteral("未配置腾讯地图 Key, 请编辑 code/client/mapconfig.h");
        return false;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), key);
    const QJsonObject root = httpGetJson(buildUrl(QStringLiteral("/ws/location/v1/ip"), query), error);
    if (root.isEmpty())
        return false;

    const int status = root.value(QStringLiteral("status")).toInt();
    if (status != 0) {
        error = root.value(QStringLiteral("message"))
                    .toString(QStringLiteral("解析失败(状态码 %1)").arg(status));
        return false;
    }

    const QJsonObject location = root.value(QStringLiteral("result")).toObject()
                                     .value(QStringLiteral("location")).toObject();
    const double lng = location.value(QStringLiteral("lng")).toDouble();
    const double lat2 = location.value(QStringLiteral("lat")).toDouble();
    if (lng == 0.0 && lat2 == 0.0) {
        error = QStringLiteral("腾讯地图未返回有效坐标");
        return false;
    }

    lon = lng;
    lat = lat2;
    return true;
}

// 腾讯位置服务 路线规划接口(驾车 / 步行)
// 文档: https://lbs.qq.com/service/webService/webServiceGuide/webServiceRoute
bool routePlan(bool driving,
               double fromLon, double fromLat,
               double toLon, double toLat,
               RouteInfo &route, QString &error)
{
    const QString key = checkKey();
    if (key.isEmpty()) {
        error = QStringLiteral("未配置腾讯地图 Key, 请编辑 code/client/mapconfig.h");
        return false;
    }

    const QString path = driving
        ? QStringLiteral("/ws/direction/v1/driving/")
        : QStringLiteral("/ws/direction/v1/walking/");

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"),
                       QStringLiteral("%1,%2").arg(fromLat, 0, 'f', 6).arg(fromLon, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("to"),
                       QStringLiteral("%1,%2").arg(toLat, 0, 'f', 6).arg(toLon, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("key"), key);

    const QJsonObject root = httpGetJson(buildUrl(path, query), error);
    if (root.isEmpty())
        return false;

    const int status = root.value(QStringLiteral("status")).toInt();
    if (status != 0) {
        error = root.value(QStringLiteral("message"))
                    .toString(QStringLiteral("解析失败(状态码 %1)").arg(status));
        return false;
    }

    const QJsonArray routes = root.value(QStringLiteral("result")).toObject()
                                  .value(QStringLiteral("routes")).toArray();
    if (routes.isEmpty()) {
        error = QStringLiteral("腾讯地图未返回路线");
        return false;
    }

    const QJsonObject r = routes.first().toObject();
    route.distanceMeters = r.value(QStringLiteral("distance")).toDouble();
    route.durationMinutes = r.value(QStringLiteral("duration")).toDouble();
    route.polyline.clear();

    // 压缩折线: 前两个是绝对(纬度, 经度), 之后每两个是 1e-6 度增量
    const QJsonArray pl = r.value(QStringLiteral("polyline")).toArray();
    if (pl.size() >= 2) {
        double lat = pl.at(0).toDouble();
        double lng = pl.at(1).toDouble();
        route.polyline.append(qMakePair(lat, lng));
        for (int i = 2; i + 1 < pl.size(); i += 2) {
            lat += pl.at(i).toDouble() / 1000000.0;
            lng += pl.at(i + 1).toDouble() / 1000000.0;
            route.polyline.append(qMakePair(lat, lng));
        }
    }

    if (route.distanceMeters <= 0.0) {
        error = QStringLiteral("腾讯地图返回距离无效");
        return false;
    }
    return true;
}

} // namespace TencentGeo
