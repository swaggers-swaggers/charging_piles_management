#include "NavigationPage.h"

#include "GeoUtil.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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
    planRow->addWidget(destLabel);
    planRow->addWidget(m_destCombo);
    planRow->addWidget(modeLabel);
    planRow->addWidget(m_modeCombo);
    planRow->addWidget(navBtn);
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

    connect(navBtn, &QPushButton::clicked, this, &NavigationPage::onPlanChanged);
    connect(m_destCombo, &QComboBox::currentIndexChanged, this, &NavigationPage::onPlanChanged);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &NavigationPage::onPlanChanged);
}

void NavigationPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到界面显示完成后再请求, 避免同步网络请求阻塞主窗口首次显示
    QTimer::singleShot(0, this, &NavigationPage::refresh);
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
    m_canvas->setData(m_stations, m_lon, m_lat, m_destIndex);
    m_canvas->update();

    if (m_destIndex < 0 || m_destIndex >= m_stations.size()) {
        m_resultLabel->setText("选择终点后点击\"开始导航\"规划路线");
        return;
    }

    const StationInfo &dest = m_stations[m_destIndex];
    // 距离取服务端计算值, 兜底本地 Haversine
    double km = dest.distance;
    if (km < 0)
        km = GeoUtil::haversineKm(m_lat, m_lon, dest.latitude, dest.longitude);

    const bool driving = (m_modeCombo->currentIndex() == 0);
    const double speedKmh = driving ? 40.0 : 5.0;
    const double hours = km / speedKmh;
    const int minutes = qMax(1, qRound(hours * 60));

    m_resultLabel->setText(QString("导航规划: → %1    |    %2    |    距离 %3 km    |    预计 %4 分%5")
                               .arg(dest.name,
                                    driving ? "驾车" : "步行",
                                    QString::number(km, 'f', 1))
                               .arg(minutes)
                               .arg(driving ? QString() : QString(" (约 %1 小时)")
                                                .arg(hours, 0, 'f', 1)));
}

// ---------- MapCanvas ----------

MapCanvas::MapCanvas(QWidget *parent)
    : QWidget(parent)
{
}

void MapCanvas::setData(const QList<StationInfo> &stations, double curLon, double curLat,
                        int destIndex)
{
    m_stations = stations;
    m_curLon = curLon;
    m_curLat = curLat;
    m_destIndex = destIndex;
}

void MapCanvas::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF area = rect().adjusted(30, 30, -30, -40);

    // 经纬度边界(加入当前位置), 各留 15% 边距
    double minLon = m_curLon, maxLon = m_curLon, minLat = m_curLat, maxLat = m_curLat;
    for (const StationInfo &s : m_stations) {
        minLon = qMin(minLon, s.longitude);
        maxLon = qMax(maxLon, s.longitude);
        minLat = qMin(minLat, s.latitude);
        maxLat = qMax(maxLat, s.latitude);
    }
    const double spanLon = qMax((maxLon - minLon) * 1.3, 0.01);
    const double spanLat = qMax((maxLat - minLat) * 1.3, 0.01);
    minLon -= (spanLon - (maxLon - minLon)) / 2;
    maxLon += (spanLon - (maxLon - minLon)) / 2;
    minLat -= (spanLat - (maxLat - minLat)) / 2;
    maxLat += (spanLat - (maxLat - minLat)) / 2;

    auto toX = [&](double lon) {
        return area.left() + (lon - minLon) / (maxLon - minLon) * area.width();
    };
    auto toY = [&](double lat) {
        return area.bottom() - (lat - minLat) / (maxLat - minLat) * area.height();
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

    // 路线(当前位置 → 终点, 折线)
    if (m_destIndex >= 0 && m_destIndex < m_stations.size()) {
        const StationInfo &dest = m_stations[m_destIndex];
        QPen routePen(QColor("#B0863F"), 3, Qt::DashLine);
        p.setPen(routePen);
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
}
