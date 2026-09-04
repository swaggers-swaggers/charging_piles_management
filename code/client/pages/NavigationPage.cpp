#include "NavigationPage.h"

#include "GeoUtil.h"
#include "TencentGeo.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>

// ---------- NavigationPage ----------

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("一键导航", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *planRow = new QHBoxLayout();
    QLabel *destLabel = new QLabel("终点:", this);
    m_destCombo = new QComboBox(this);
    m_destCombo->setObjectName("destCombo");
    m_destCombo->setMinimumWidth(220);
    QLabel *modeLabel = new QLabel("出行方式:", this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName("modeCombo");
    m_modeCombo->addItem("驾车");
    m_modeCombo->addItem("步行");
    QPushButton *navBtn = new QPushButton("开始导航", this);
    navBtn->setObjectName("primaryBtn");
    QPushButton *locateBtn = new QPushButton("获取当前位置", this);
    locateBtn->setObjectName("secondaryBtn");
    QPushButton *zoomInBtn = new QPushButton("＋", this);
    zoomInBtn->setObjectName("secondaryBtn");
    zoomInBtn->setToolTip("放大地图");
    zoomInBtn->setFixedWidth(32);
    QPushButton *zoomOutBtn = new QPushButton("－", this);
    zoomOutBtn->setObjectName("secondaryBtn");
    zoomOutBtn->setToolTip("缩小地图");
    zoomOutBtn->setFixedWidth(32);
    planRow->addWidget(destLabel);
    planRow->addWidget(m_destCombo);
    planRow->addWidget(modeLabel);
    planRow->addWidget(m_modeCombo);
    planRow->addWidget(navBtn);
    planRow->addWidget(locateBtn);
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

    connect(navBtn, &QPushButton::clicked, this, &NavigationPage::onNavigate);
    connect(locateBtn, &QPushButton::clicked, this, &NavigationPage::onLocate);
    connect(zoomInBtn, &QPushButton::clicked, m_canvas, &MapCanvas::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, m_canvas, &MapCanvas::zoomOut);
    connect(m_destCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NavigationPage::onPlanChanged);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &NavigationPage::onPlanChanged);
}

void NavigationPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 首次进入页面时自动 IP 定位一次(失败会保留默认值, 可手动点击"获取当前位置"重试)
    if (!m_autoLocated) {
        m_autoLocated = true;
        onLocate();
        return;
    }
    // 延迟到界面显示完成后再请求, 避免同步网络请求阻塞主窗口首次显示
    QTimer::singleShot(0, this, &NavigationPage::refresh);
}

void NavigationPage::onLocate()
{
    QString err;
    if (!TencentGeo::ipLocation(m_lon, m_lat, err)) {
        // IP 定位失败(如当日额度用完/断网)时回退到默认位置, 不再中断流程
        m_lon = 116.3100;
        m_lat = 39.9600;
        m_resultLabel->setText(QString("IP定位不可用(%1), 已回退到默认位置(北京海淀)")
                                   .arg(err));
        refresh();
        return;
    }
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

    const QString saved = m_destCombo->currentText();
    m_destCombo->blockSignals(true);
    m_destCombo->clear();
    for (const StationInfo &s : m_stations)
        m_destCombo->addItem(QString("%1 (%2 km)").arg(s.name).arg(s.distance, 0, 'f', 1));
    m_destCombo->blockSignals(false);
    const int idx = m_destCombo->findText(saved);
    if (idx >= 0)
        m_destCombo->setCurrentIndex(idx);

    onPlanChanged();
}

void NavigationPage::onPlanChanged()
{
    m_destIndex = m_destCombo->currentIndex();
    m_routePolyline.clear();
    m_canvas->setData(m_stations, m_lon, m_lat, m_destIndex, m_routePolyline);
    m_canvas->update();

    if (m_destIndex < 0 || m_destIndex >= m_stations.size()) {
        m_resultLabel->setText("选择终点后点击\"开始导航\"规划路线");
        return;
    }

    // 仅展示直线距离估算, 不调用腾讯接口(省额度); 真实路线由"开始导航"触发
    const StationInfo &dest = m_stations[m_destIndex];
    const bool driving = (m_modeCombo->currentIndex() == 0);
    double km = dest.distance;
    if (km < 0)
        km = GeoUtil::haversineKm(m_lat, m_lon, dest.latitude, dest.longitude);
    m_resultLabel->setText(QString("已选终点: %1 (直线约 %2 km), 点击\"开始导航\"获取 %3 实时路线")
                               .arg(dest.name)
                               .arg(QString::number(km, 'f', 1))
                               .arg(driving ? "驾车" : "步行"));
}

void NavigationPage::onNavigate()
{
    m_destIndex = m_destCombo->currentIndex();
    if (m_destIndex < 0 || m_destIndex >= m_stations.size()) {
        m_resultLabel->setText("请先选择终点");
        return;
    }

    const StationInfo &dest = m_stations[m_destIndex];
    const bool driving = (m_modeCombo->currentIndex() == 0);

    // 调用腾讯路线规划获取真实道路折线/距离/时长
    TencentGeo::RouteInfo route;
    QString routeErr;
    if (TencentGeo::routePlan(driving, m_lon, m_lat, dest.longitude, dest.latitude,
                              route, routeErr)) {
        m_routePolyline = route.polyline;
        m_canvas->setData(m_stations, m_lon, m_lat, m_destIndex, m_routePolyline);
        m_canvas->update();

        const double km = route.distanceMeters / 1000.0;
        const int minutes = qMax(1, qRound(route.durationMinutes));
        m_resultLabel->setText(QString("导航规划(腾讯实时路线): → %1    |    %2    |    距离 %3 km    |    预计 %4 分%5")
                                   .arg(dest.name,
                                        driving ? "驾车" : "步行",
                                        QString::number(km, 'f', 1))
                                   .arg(minutes)
                                   .arg(driving ? QString() : QString(" (约 %1 小时)")
                                                    .arg(route.durationMinutes / 60.0, 0, 'f', 1)));
        return;
    }

    // 兜底: 路线规划失败(如当日额度用完/断网)时, 退回直线 + Haversine 估算
    double km = dest.distance;
    if (km < 0)
        km = GeoUtil::haversineKm(m_lat, m_lon, dest.latitude, dest.longitude);

    const double speedKmh = driving ? 40.0 : 5.0;
    const double hours = km / speedKmh;
    const int minutes = qMax(1, qRound(hours * 60));

    m_routePolyline.clear();
    m_canvas->setData(m_stations, m_lon, m_lat, m_destIndex, m_routePolyline);
    m_canvas->update();
    m_resultLabel->setText(QString("导航规划(直线估算): → %1    |    %2    |    距离 %3 km    |    预计 %4 分")
                               .arg(dest.name,
                                    driving ? "驾车" : "步行",
                                    QString::number(km, 'f', 1))
                               .arg(minutes));
}

// ---------- MapCanvas ----------

MapCanvas::MapCanvas(QWidget *parent)
    : QWidget(parent)
{
}

void MapCanvas::setData(const QList<StationInfo> &stations, double curLon, double curLat,
                        int destIndex, const QList<QPair<double, double>> &routePolyline)
{
    // 当前位置或站点数量变化时重置视野; 仅切换终点/路线时不重置, 保留用户缩放状态
    const bool dataChanged = (m_curLon != curLon) || (m_curLat != curLat)
                             || (m_stations.size() != stations.size());

    m_stations = stations;
    m_curLon = curLon;
    m_curLat = curLat;
    m_destIndex = destIndex;
    m_route = routePolyline;

    updateBaseBounds();
    if (dataChanged) {
        m_centerLon = m_baseCenterLon;
        m_centerLat = m_baseCenterLat;
        m_zoom = 1.0;
    }
}

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
    zoomAt(QPointF(rect().center()), 1.25);
}

void MapCanvas::zoomOut()
{
    zoomAt(QPointF(rect().center()), 1.0 / 1.25);
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
}
