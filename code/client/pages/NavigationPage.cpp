#include "NavigationPage.h"

#include "GeoUtil.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>

#ifdef CHARGING_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#endif

namespace {

struct DemoLocation {
    const char *name;
    double lon;
    double lat;
};

// 学生桌面项目没有真实 GPS，使用显式可控的演示起点比 IP 定位更稳定、可复现。
const DemoLocation kDemoLocations[] = {
    { "海淀区·五道口", 116.3391, 39.9911 },
    { "海淀区·中关村", 116.3160, 39.9830 },
    { "朝阳区·国贸", 116.4610, 39.9087 },
    { "朝阳区·望京", 116.4740, 39.9960 },
    { "东城区·北京站", 116.4270, 39.9030 },
    { "西城区·西单", 116.3740, 39.9130 },
    { "丰台区·北京南站", 116.3785, 39.8655 },
    { "大兴区·大兴机场", 116.4204, 39.5295 },
};

QString coordinateText(double value)
{
    return QString::number(value, 'f', 6);
}

} // namespace

// ---------- NavigationPage ----------

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("一键导航", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *planRow = new QHBoxLayout();
    QLabel *startLabel = new QLabel("起点:", this);
    m_startCombo = new QComboBox(this);
    m_startCombo->setObjectName("startCombo");
    for (const DemoLocation &location : kDemoLocations)
        m_startCombo->addItem(QString::fromUtf8(location.name));

    QLabel *destLabel = new QLabel("终点:", this);
    m_destCombo = new QComboBox(this);
    m_destCombo->setObjectName("destCombo");
    m_destCombo->setMinimumWidth(220);
    QLabel *modeLabel = new QLabel("出行方式:", this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName("modeCombo");
    m_modeCombo->addItem("驾车");
    m_modeCombo->addItem("步行");
    m_previewButton = new QPushButton("路线预览", this);
    m_previewButton->setObjectName("secondaryBtn");
    m_externalButton = new QPushButton("开始导航", this);
    m_externalButton->setObjectName("primaryBtn");
    QPushButton *zoomInBtn = new QPushButton("＋", this);
    zoomInBtn->setObjectName("secondaryBtn");
    zoomInBtn->setToolTip("放大地图");
    zoomInBtn->setFixedWidth(32);
    QPushButton *zoomOutBtn = new QPushButton("－", this);
    zoomOutBtn->setObjectName("secondaryBtn");
    zoomOutBtn->setToolTip("缩小地图");
    zoomOutBtn->setFixedWidth(32);
    planRow->addWidget(startLabel);
    planRow->addWidget(m_startCombo);
    planRow->addWidget(destLabel);
    planRow->addWidget(m_destCombo);
    planRow->addWidget(modeLabel);
    planRow->addWidget(m_modeCombo);
    planRow->addWidget(m_previewButton);
    planRow->addWidget(m_externalButton);
    planRow->addWidget(zoomInBtn);
    planRow->addWidget(zoomOutBtn);
    planRow->addStretch();

    m_canvas = new MapCanvas(this);
    m_canvas->setObjectName("mapCanvas");
    m_canvas->setMinimumHeight(360);

    m_resultLabel = new QLabel("选择终点后点击\"开始导航\"规划路线", this);
    m_resultLabel->setObjectName("navResult");
    m_resultLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addLayout(planRow);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(m_resultLabel);

    connect(m_startCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &NavigationPage::onStartChanged);
    connect(m_previewButton, &QPushButton::clicked, this, &NavigationPage::onPreviewRoute);
    connect(m_externalButton, &QPushButton::clicked,
            this, &NavigationPage::onOpenExternalNavigation);
    connect(zoomInBtn, &QPushButton::clicked, m_canvas, &MapCanvas::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, m_canvas, &MapCanvas::zoomOut);
    connect(m_destCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NavigationPage::onPlanChanged);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NavigationPage::onPlanChanged);

    onStartChanged(0);
}

void NavigationPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到界面显示完成后查询站点；不再进入页面就消耗一次第三方定位额度。
    QTimer::singleShot(0, this, &NavigationPage::refresh);
}

void NavigationPage::onStartChanged(int index)
{
    if (index < 0 || index >= int(sizeof(kDemoLocations) / sizeof(kDemoLocations[0])))
        return;

    if (m_routeReply) {
        disconnect(m_routeReply, nullptr, this, nullptr);
        m_routeReply->abort();
        m_routeReply->deleteLater();
        m_routeReply = nullptr;
        m_previewButton->setEnabled(true);
    }
    m_lon = kDemoLocations[index].lon;
    m_lat = kDemoLocations[index].lat;
    m_routePolyline.clear();
    if (isVisible())
        refresh();
}

void NavigationPage::refresh()
{
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationList, QJsonObject{{"lon", m_lon}, {"lat", m_lat}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "查询失败", reply.value("error").toString());
        return;
    }

    m_stations.clear();
    const QJsonArray arr = reply.value("stations").toArray();
    for (const QJsonValue &v : arr)
        m_stations.append(StationInfo::fromJson(v.toObject()));

    const int savedStationId = m_destCombo->currentData().toInt();
    m_destCombo->blockSignals(true);
    m_destCombo->clear();
    for (const StationInfo &s : m_stations)
        m_destCombo->addItem(QString("%1 (%2 km)").arg(s.name).arg(s.distance, 0, 'f', 1), s.id);
    m_destCombo->blockSignals(false);
    const int idx = m_destCombo->findData(savedStationId);
    if (idx >= 0)
        m_destCombo->setCurrentIndex(idx);

    onPlanChanged();
}

void NavigationPage::onPlanChanged()
{
    if (m_routeReply) {
        disconnect(m_routeReply, nullptr, this, nullptr);
        m_routeReply->abort();
        m_routeReply->deleteLater();
        m_routeReply = nullptr;
        m_previewButton->setEnabled(true);
    }
    m_destIndex = m_destCombo->currentIndex();
    m_routePolyline.clear();
    m_canvas->setData(m_stations, m_lon, m_lat, m_destIndex, m_routePolyline);
    m_canvas->update();

    const bool validDestination = m_destIndex >= 0 && m_destIndex < m_stations.size();
    m_previewButton->setEnabled(validDestination);
    m_externalButton->setEnabled(validDestination);
    if (!validDestination) {
        m_resultLabel->setText("选择终点后点击\"开始导航\"规划路线");
        return;
    }

    // 切换选项不发起外网请求；只有用户点击“路线预览”时才调用开放路由服务。
    const StationInfo &dest = m_stations[m_destIndex];
    const bool driving = (m_modeCombo->currentIndex() == 0);
    double km = dest.distance;
    if (km < 0)
        km = GeoUtil::haversineKm(m_lat, m_lon, dest.latitude, dest.longitude);
    m_resultLabel->setText(QString("已选终点: %1（直线约 %2 km）｜可预览路线或打开高德 %3 导航")
                               .arg(dest.name)
                               .arg(QString::number(km, 'f', 1))
                               .arg(driving ? "驾车" : "步行"));
}

void NavigationPage::onPreviewRoute()
{
    m_destIndex = m_destCombo->currentIndex();
    if (m_destIndex < 0 || m_destIndex >= m_stations.size()) {
        m_resultLabel->setText("请先选择终点");
        return;
    }

    const StationInfo &dest = m_stations[m_destIndex];
    const bool driving = (m_modeCombo->currentIndex() == 0);

    // FOSSGIS 公共演示服务要求每秒最多一次请求。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastRouteRequestAt < 1000) {
        m_resultLabel->setText("路线预览请求请至少间隔 1 秒");
        return;
    }
    m_lastRouteRequestAt = now;

    double fromWgsLon = 0.0;
    double fromWgsLat = 0.0;
    double toWgsLon = 0.0;
    double toWgsLat = 0.0;
    GeoUtil::gcj02ToWgs84(m_lon, m_lat, fromWgsLon, fromWgsLat);
    GeoUtil::gcj02ToWgs84(dest.longitude, dest.latitude, toWgsLon, toWgsLat);

    const QString profile = driving ? QStringLiteral("routed-car") : QStringLiteral("routed-foot");
    QUrl routeUrl(QStringLiteral("https://routing.openstreetmap.de/%1/route/v1/driving/"
                                 "%2,%3;%4,%5")
                      .arg(profile,
                           coordinateText(fromWgsLon), coordinateText(fromWgsLat),
                           coordinateText(toWgsLon), coordinateText(toWgsLat)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("overview"), QStringLiteral("full"));
    query.addQueryItem(QStringLiteral("geometries"), QStringLiteral("geojson"));
    query.addQueryItem(QStringLiteral("steps"), QStringLiteral("false"));
    routeUrl.setQuery(query);

    QNetworkRequest request(routeUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("ChargingPlatform-StudentProject/1.0"));
    request.setRawHeader("Accept", "application/json");
    m_routeReply = m_networkManager->get(request);
    connect(m_routeReply, &QNetworkReply::finished,
            this, &NavigationPage::onRouteReplyFinished);
    QTimer::singleShot(10000, m_routeReply, [reply = m_routeReply]() {
        if (reply->isRunning())
            reply->abort();
    });
    m_previewButton->setEnabled(false);
    m_resultLabel->setText(QString("正在获取%1路线预览……").arg(driving ? "驾车" : "步行"));
}

void NavigationPage::onRouteReplyFinished()
{
    QNetworkReply *reply = m_routeReply;
    m_routeReply = nullptr;
    m_previewButton->setEnabled(true);
    if (!reply)
        return;

    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    const QJsonArray routes = document.object().value(QStringLiteral("routes")).toArray();
    if (networkError != QNetworkReply::NoError || httpStatus != 200
        || parseError.error != QJsonParseError::NoError || routes.isEmpty()) {
        const QString detail = networkError != QNetworkReply::NoError
            ? networkErrorText
            : QStringLiteral("服务返回 HTTP %1").arg(httpStatus);
        m_resultLabel->setText(QString("路线预览暂不可用（%1），仍可点击“开始导航”打开高德")
                                   .arg(detail));
        return;
    }

    const QJsonObject route = routes.first().toObject();
    const QJsonArray coordinates = route.value(QStringLiteral("geometry")).toObject()
                                       .value(QStringLiteral("coordinates")).toArray();
    m_routePolyline.clear();
    for (const QJsonValue &value : coordinates) {
        const QJsonArray point = value.toArray();
        if (point.size() >= 2)
            m_routePolyline.append(qMakePair(point.at(1).toDouble(), point.at(0).toDouble()));
    }
    if (m_routePolyline.size() < 2) {
        m_resultLabel->setText("路线服务未返回有效折线，仍可点击“开始导航”打开高德");
        return;
    }

    m_canvas->setData(m_stations, m_lon, m_lat, m_destIndex, m_routePolyline);
    m_canvas->update();

    const double km = route.value(QStringLiteral("distance")).toDouble() / 1000.0;
    const int minutes = qMax(1, qRound(route.value(QStringLiteral("duration")).toDouble() / 60.0));
    const QString destinationName = m_destIndex >= 0 && m_destIndex < m_stations.size()
        ? m_stations[m_destIndex].name : QStringLiteral("目的地");
    m_resultLabel->setText(QString("开放地图路线预览：→ %1｜%2｜%3 km｜预计 %4 分钟")
                               .arg(destinationName,
                                    m_modeCombo->currentIndex() == 0 ? "驾车" : "步行",
                                    QString::number(km, 'f', 1))
                               .arg(minutes));
}

void NavigationPage::onOpenExternalNavigation()
{
    m_destIndex = m_destCombo->currentIndex();
    if (m_destIndex < 0 || m_destIndex >= m_stations.size()) {
        m_resultLabel->setText("请先选择终点");
        return;
    }

    const StationInfo &dest = m_stations[m_destIndex];
    QUrl url(QStringLiteral("https://uri.amap.com/navigation"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"),
                       QStringLiteral("%1,%2,当前位置")
                           .arg(coordinateText(m_lon), coordinateText(m_lat)));
    query.addQueryItem(QStringLiteral("to"),
                       QStringLiteral("%1,%2,%3")
                           .arg(coordinateText(dest.longitude),
                                coordinateText(dest.latitude), dest.name));
    query.addQueryItem(QStringLiteral("mode"),
                       m_modeCombo->currentIndex() == 0 ? QStringLiteral("car")
                                                        : QStringLiteral("walk"));
    query.addQueryItem(QStringLiteral("src"), QStringLiteral("ChargingPlatform"));
    query.addQueryItem(QStringLiteral("callnative"), QStringLiteral("0"));
    url.setQuery(query);

    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, "无法打开导航",
                             "系统未能打开浏览器，请检查默认浏览器设置。");
        return;
    }
    m_resultLabel->setText(QString("已打开高德导航：%1 → %2")
                               .arg(m_startCombo->currentText(), dest.name));
}

// ---------- MapCanvas ----------

MapCanvas::MapCanvas(QWidget *parent)
    : QWidget(parent)
{
#ifdef CHARGING_HAS_WEBENGINE
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_webView = new QWebEngineView(this);
    m_webView->setContextMenuPolicy(Qt::NoContextMenu);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    layout->addWidget(m_webView);
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_webReady = ok;
        if (m_webReady && !m_pendingScript.isEmpty()) {
            m_webView->page()->runJavaScript(m_pendingScript);
            m_pendingScript.clear();
        }
    });
    m_webView->load(QUrl(QStringLiteral("qrc:/map/map.html")));
#endif
}

void MapCanvas::setData(const QList<StationInfo> &stations, double curLon, double curLat,
                        int destIndex, const QList<QPair<double, double>> &routePolyline)
{
    double wgsCurLon = 0.0;
    double wgsCurLat = 0.0;
    GeoUtil::gcj02ToWgs84(curLon, curLat, wgsCurLon, wgsCurLat);

    QList<StationInfo> wgsStations = stations;
    for (StationInfo &station : wgsStations) {
        double wgsLon = 0.0;
        double wgsLat = 0.0;
        GeoUtil::gcj02ToWgs84(station.longitude, station.latitude, wgsLon, wgsLat);
        station.longitude = wgsLon;
        station.latitude = wgsLat;
    }

    // 当前位置或站点数量变化时重置视野；路线返回时自动缩放到路线范围。
    const bool dataChanged = (m_curLon != wgsCurLon) || (m_curLat != wgsCurLat)
                             || (m_stations.size() != wgsStations.size());
#ifdef CHARGING_HAS_WEBENGINE
    const bool routeChanged = (m_route != routePolyline);
#endif

    m_stations = wgsStations;
    m_curLon = wgsCurLon;
    m_curLat = wgsCurLat;
    m_destIndex = destIndex;
    m_route = routePolyline;

    updateBaseBounds();
    if (dataChanged) {
        m_centerLon = m_baseCenterLon;
        m_centerLat = m_baseCenterLat;
        m_zoom = 1.0;
    }
#ifdef CHARGING_HAS_WEBENGINE
    updateWebMap(dataChanged || (routeChanged && m_route.size() >= 2));
#endif
}

#ifdef CHARGING_HAS_WEBENGINE
void MapCanvas::updateWebMap(bool fitView)
{
    QJsonObject root;
    root.insert(QStringLiteral("current"),
                QJsonObject{{QStringLiteral("lon"), m_curLon},
                            {QStringLiteral("lat"), m_curLat}});
    root.insert(QStringLiteral("destinationIndex"), m_destIndex);
    root.insert(QStringLiteral("fitView"), fitView);

    QJsonArray stations;
    for (const StationInfo &station : m_stations) {
        stations.append(QJsonObject{
            {QStringLiteral("name"), station.name},
            {QStringLiteral("address"), station.address},
            {QStringLiteral("lon"), station.longitude},
            {QStringLiteral("lat"), station.latitude},
            {QStringLiteral("price"), station.price},
            {QStringLiteral("idlePiles"), station.idlePiles},
            {QStringLiteral("totalPiles"), station.totalPiles},
            {QStringLiteral("distance"), station.distance}
        });
    }
    root.insert(QStringLiteral("stations"), stations);

    QJsonArray route;
    for (const QPair<double, double> &point : m_route)
        route.append(QJsonArray{point.second, point.first});
    root.insert(QStringLiteral("route"), route);

    const QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    m_pendingScript = QStringLiteral("window.setChargingMapData(%1);").arg(json);
    if (m_webReady) {
        m_webView->page()->runJavaScript(m_pendingScript);
        m_pendingScript.clear();
    }
}
#endif

void MapCanvas::updateBaseBounds()
{
    double minLon = m_curLon, maxLon = m_curLon, minLat = m_curLat, maxLat = m_curLat;
    for (const StationInfo &s : m_stations) {
        minLon = qMin(minLon, s.longitude);
        maxLon = qMax(maxLon, s.longitude);
        minLat = qMin(minLat, s.latitude);
        maxLat = qMax(maxLat, s.latitude);
    }
    m_baseSpanLon = qMax((maxLon - minLon) * 1.3, 0.01);
    m_baseSpanLat = qMax((maxLat - minLat) * 1.3, 0.01);
    m_baseCenterLon = (minLon + maxLon) / 2.0;
    m_baseCenterLat = (minLat + maxLat) / 2.0;
}

void MapCanvas::zoomAt(const QPointF &cursorPos, double factor)
{
    const QRectF area = rect().adjusted(30, 30, -30, -40);
    // 光标在视野内的归一化位置(0~1), 用于计算光标下的地理点
    double fx = (cursorPos.x() - area.left()) / area.width();
    double fy = (cursorPos.y() - area.top()) / area.height();
    fx = qBound(0.0, fx, 1.0);
    fy = qBound(0.0, fy, 1.0);

    const double oldSpanLon = m_baseSpanLon / m_zoom;
    const double oldSpanLat = m_baseSpanLat / m_zoom;
    // 光标下的地理坐标
    const double geoLon = m_centerLon + (fx - 0.5) * oldSpanLon;
    const double geoLat = m_centerLat + (0.5 - fy) * oldSpanLat;

    m_zoom = qBound(0.5, m_zoom * factor, 8.0);

    const double newSpanLon = m_baseSpanLon / m_zoom;
    const double newSpanLat = m_baseSpanLat / m_zoom;
    // 调整视野中心, 使光标下的地理点保持不动
    m_centerLon = geoLon - (fx - 0.5) * newSpanLon;
    m_centerLat = geoLat - (0.5 - fy) * newSpanLat;

    update();
}

void MapCanvas::zoomIn()
{
#ifdef CHARGING_HAS_WEBENGINE
    if (m_webReady)
        m_webView->page()->runJavaScript(QStringLiteral("window.chargingMapZoomIn();"));
    return;
#else
    zoomAt(QPointF(rect().center()), 1.25);
#endif
}

void MapCanvas::zoomOut()
{
#ifdef CHARGING_HAS_WEBENGINE
    if (m_webReady)
        m_webView->page()->runJavaScript(QStringLiteral("window.chargingMapZoomOut();"));
    return;
#else
    zoomAt(QPointF(rect().center()), 1.0 / 1.25);
#endif
}

void MapCanvas::wheelEvent(QWheelEvent *event)
{
    const double delta = event->angleDelta().y();
    if (delta > 0)
        zoomAt(event->position(), 1.25);
    else if (delta < 0)
        zoomAt(event->position(), 1.0 / 1.25);
    event->accept();
}

void MapCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_dragStartCenterLon = m_centerLon;
        m_dragStartCenterLat = m_centerLat;
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void MapCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPointF d = event->pos() - m_dragStart;
        const QRectF area = rect().adjusted(30, 30, -30, -40);
        const double spanLon = m_baseSpanLon / m_zoom;
        const double spanLat = m_baseSpanLat / m_zoom;
        // 拖拽地图: 鼠标移动方向与视野中心移动方向相反(地图跟随鼠标)
        m_centerLon = m_dragStartCenterLon - d.x() * spanLon / area.width();
        m_centerLat = m_dragStartCenterLat + d.y() * spanLat / area.height();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        unsetCursor();
    }
    QWidget::mouseReleaseEvent(event);
}

void MapCanvas::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
#ifdef CHARGING_HAS_WEBENGINE
    Q_UNUSED(event);
    return;
#else
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF area = rect().adjusted(30, 30, -30, -40);

    // 视野: 以 m_centerLon/Lat 为中心(随鼠标缩放而移动), 按 m_zoom 缩放
    const double spanLon = m_baseSpanLon / m_zoom;
    const double spanLat = m_baseSpanLat / m_zoom;
    const double dMinLon = m_centerLon - spanLon / 2.0;
    const double dMaxLon = m_centerLon + spanLon / 2.0;
    const double dMinLat = m_centerLat - spanLat / 2.0;
    const double dMaxLat = m_centerLat + spanLat / 2.0;

    auto toX = [&](double lon) {
        return area.left() + (lon - dMinLon) / (dMaxLon - dMinLon) * area.width();
    };
    auto toY = [&](double lat) {
        return area.bottom() - (lat - dMinLat) / (dMaxLat - dMinLat) * area.height();
    };

    // 网格(模拟地图街区)
    p.setPen(QPen(QColor("#D8DFE8"), 1));
    for (int i = 0; i <= 8; ++i) {
        const double x = area.left() + area.width() * i / 8;
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
    for (int i = 0; i <= 6; ++i) {
        const double y = area.top() + area.height() * i / 6;
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    // 路线: 有真实折线时画道路折线, 否则退回直线
    QPen routePen(QColor("#B0863F"), 3);
    p.setPen(routePen);
    if (m_route.size() >= 2) {
        QVector<QPointF> pts;
        pts.reserve(m_route.size());
        for (const QPair<double, double> &pt : m_route)
            pts.append(QPointF(toX(pt.second), toY(pt.first)));   // (纬度, 经度) -> (lon, lat)
        p.drawPolyline(pts.constData(), pts.size());
    } else if (m_destIndex >= 0 && m_destIndex < m_stations.size()) {
        const StationInfo &dest = m_stations[m_destIndex];
        QPen dashPen(QColor("#B0863F"), 3, Qt::DashLine);
        p.setPen(dashPen);
        p.drawLine(QPointF(toX(m_curLon), toY(m_curLat)),
                   QPointF(toX(dest.longitude), toY(dest.latitude)));
    }

    // 充电站散点
    p.setFont(font());
    for (int i = 0; i < m_stations.size(); ++i) {
        const StationInfo &s = m_stations[i];
        const QPointF pt(toX(s.longitude), toY(s.latitude));
        const bool isDest = (i == m_destIndex);
        p.setBrush(QColor(isDest ? "#C5525A" : "#1F9D67"));
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(pt, isDest ? 9 : 7, isDest ? 9 : 7);
        p.setPen(QColor("#3A4758"));
        p.drawText(QPointF(pt.x() + 12, pt.y() + 4), s.name);
    }

    // 当前位置(定位圆点)
    const QPointF cur(toX(m_curLon), toY(m_curLat));
    p.setPen(QPen(QColor("#B0863F"), 2));
    p.setBrush(QColor("#B0863F"));
    p.drawEllipse(cur, 6, 6);
    p.setBrush(QColor(176, 134, 63, 60));
    p.drawEllipse(cur, 14, 14);
    p.setPen(QColor("#A9864F"));
    p.drawText(QPointF(cur.x() - 24, cur.y() - 20), "当前位置");

    // 缩放倍率提示(右上角)
    p.setPen(QColor("#7A8BA0"));
    p.drawText(QRectF(area.left(), area.top() - 24, area.width(), 20),
               Qt::AlignRight | Qt::AlignTop,
               QString("缩放 %1%").arg(qRound(m_zoom * 100)));
#endif
}
