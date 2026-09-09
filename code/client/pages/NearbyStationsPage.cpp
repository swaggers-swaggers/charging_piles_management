#include "NearbyStationsPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include "AdminTableCard.h"
#include <QScrollArea>
#include <QCheckBox>
#include <QFrame>
#include <QDateTime>
#include <algorithm>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QTableWidget>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QtMath>

namespace {
class DiscoveryTerrainHero : public QFrame
{
public:
    explicit DiscoveryTerrainHero(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("discoveryTerrainHero"));
        setAccessibleName(QStringLiteral("等高线能量地形"));
        setProperty("terrainStyle", QStringLiteral("topographic-relief"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setMinimumHeight(148);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_pulse.setDuration(2800);
        m_pulse.setStartValue(0.0);
        m_pulse.setEndValue(1.0);
        m_pulse.setLoopCount(-1);
        m_pulse.setEasingCurve(QEasingCurve::InOutSine);
        connect(&m_pulse, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) {
            m_phase = value.toReal();
            update();
        });
        m_pulse.start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath clip;
        clip.addRoundedRect(bounds, 18, 18);
        painter.setClipPath(clip);

        QLinearGradient base(bounds.topLeft(), bounds.bottomRight());
        base.setColorAt(0.0, QColor("#0B3E37"));
        base.setColorAt(0.52, QColor("#0F5547"));
        base.setColorAt(1.0, QColor("#123F38"));
        painter.fillPath(clip, base);

        QRadialGradient horizon(QPointF(width() * 0.72, height() * 0.06), width() * 0.48);
        horizon.setColorAt(0.0, QColor(167, 232, 196, 76));
        horizon.setColorAt(0.45, QColor(91, 169, 132, 28));
        horizon.setColorAt(1.0, QColor(15, 76, 64, 0));
        painter.fillRect(rect(), horizon);

        const auto terrainY = [this](qreal x, int layer) {
            const qreal t = width() > 0 ? x / width() : 0.0;
            const qreal wave = qSin(t * 6.28318530718 + layer * 0.62) * (8.0 + layer)
                + qSin(t * 12.56637061436 + layer * 0.91) * 3.8;
            const qreal ridge = qExp(-qPow((t - 0.19) / 0.13, 2.0)) * (22.0 - layer * 1.8)
                - qExp(-qPow((t - 0.70) / 0.20, 2.0)) * (13.0 - layer);
            return height() * (0.34 + layer * 0.092) + wave + ridge;
        };

        // 深浅相叠的山脊形成纸雕般的层次，每层轮廓保持非对称。
        for (int layer = 0; layer < 6; ++layer) {
            QPainterPath band;
            band.moveTo(-8, terrainY(-8, layer));
            for (int x = 0; x <= width() + 8; x += 9)
                band.lineTo(x, terrainY(x, layer));
            band.lineTo(width() + 8, height() + 8);
            band.lineTo(-8, height() + 8);
            band.closeSubpath();
            painter.fillPath(band, QColor(5 + layer * 3, 47 + layer * 5,
                                          41 + layer * 5, 172));

            QPainterPath edge;
            edge.moveTo(-8, terrainY(-8, layer));
            for (int x = 0; x <= width() + 8; x += 9)
                edge.lineTo(x, terrainY(x, layer));
            painter.setPen(QPen(QColor(126, 207, 170, 34 + layer * 5), 0.85));
            painter.drawPath(edge);
        }

        // 更细的等高线漂浮在地形之上。
        for (int line = 0; line < 10; ++line) {
            QPainterPath contour;
            const qreal baseY = height() * (0.16 + line * 0.061);
            for (int x = -8; x <= width() + 8; x += 8) {
                const qreal t = width() > 0 ? qreal(x) / width() : 0.0;
                const qreal y = baseY
                    + qSin(t * 7.2 + line * 0.48) * (7.0 + line * 0.45)
                    + qSin(t * 15.0 + line * 0.77) * 2.4;
                if (x == -8) contour.moveTo(x, y); else contour.lineTo(x, y);
            }
            painter.setPen(QPen(QColor(157, 224, 193, 25 + line * 3), 0.75));
            painter.drawPath(contour);
        }

        const QPointF node(width() * 0.68, height() * 0.52);
        QPainterPath energy;
        energy.moveTo(width() * 0.34, height() * 0.92);
        energy.cubicTo(width() * 0.48, height() * 0.90,
                       width() * 0.55, height() * 0.46, node.x(), node.y());
        energy.cubicTo(width() * 0.79, height() * 0.63,
                       width() * 0.86, height() * 0.34,
                       width() * 1.02, height() * 0.43);
        painter.setPen(QPen(QColor(99, 244, 181, 34), 12,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(energy);
        painter.setPen(QPen(QColor(129, 251, 198, 104), 4.2,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(energy);
        painter.setPen(QPen(QColor("#C6FFE5"), 1.5,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(energy);

        const qreal glowRadius = 18.0 + 6.0 * qSin(m_phase * 3.14159265359);
        QRadialGradient glow(node, glowRadius);
        glow.setColorAt(0.0, QColor(214, 255, 234, 210));
        glow.setColorAt(0.3, QColor(86, 242, 171, 145));
        glow.setColorAt(1.0, QColor(86, 242, 171, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(node, glowRadius, glowRadius);
        painter.setBrush(QColor("#D8FFEA"));
        painter.drawEllipse(node, 5.5, 5.5);
        painter.setBrush(QColor("#4FE3A0"));
        painter.drawEllipse(node, 2.8, 2.8);

        // 在留白区域放置页面名称，与能量路线保持清晰的前后层次。
        const QRectF titlePanel(28, 24, qMin<qreal>(250, width() * 0.34), 64);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(4, 45, 39, 154));
        painter.drawRoundedRect(titlePanel, 12, 12);
        painter.setBrush(QColor("#57E2A5"));
        painter.drawRoundedRect(QRectF(titlePanel.left() + 16, titlePanel.top() + 15,
                                       3, titlePanel.height() - 30), 1.5, 1.5);

        QFont titleFont = painter.font();
        titleFont.setPixelSize(width() < 720 ? 20 : 24);
        titleFont.setWeight(QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(QColor("#F2FFF8"));
        painter.drawText(titlePanel.adjusted(31, 0, -12, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QStringLiteral("附近充电桩"));

        painter.setClipping(false);
        painter.setPen(QPen(QColor(197, 239, 219, 58), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bounds, 18, 18);
    }

    void showEvent(QShowEvent *event) override
    {
        QFrame::showEvent(event);
        m_pulse.start();
    }

    void hideEvent(QHideEvent *event) override
    {
        QFrame::hideEvent(event);
        m_pulse.stop();
    }

private:
    QVariantAnimation m_pulse;
    qreal m_phase = 0.0;
};

// 模拟GPS: 区域 → 固定经纬度(北京市主要城区)
struct RegionCoord {
    const char *name;
    double lon;
    double lat;
};
const RegionCoord kRegions[] = {
    { "海淀区(默认)",  116.3100, 39.9600 },
    { "朝阳区",        116.4430, 39.9210 },
    { "东城区",        116.4160, 39.9160 },
    { "西城区",        116.3660, 39.9150 },
    { "丰台区",        116.2860, 39.8580 },
    { "大兴区",        116.3410, 39.7280 },
    { "通州区",        116.6560, 39.9100 },
};

} // namespace

NearbyStationsPage::NearbyStationsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    auto *hero = new DiscoveryTerrainHero(this);
    layout->addWidget(hero);

    m_summary = new QLabel("正在发现附近充电站…", this);
    m_summary->setObjectName("discoverySummary");
    layout->addWidget(m_summary);

    QHBoxLayout *filterRow = new QHBoxLayout();
    m_regionCombo = new QComboBox(this);
    m_regionCombo->setObjectName("regionCombo");
    m_regionCombo->setAccessibleName("位置区域");
    for (const RegionCoord &r : kRegions)
        m_regionCombo->addItem(QString::fromUtf8(r.name));
    QPushButton *refreshBtn = new QPushButton("刷新站点", this);
    refreshBtn->setObjectName("secondaryBtn");
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("搜索站名或地址");
    m_search->setAccessibleName("搜索站名或地址");
    m_search->setClearButtonEnabled(true);
    m_idleOnly = new QCheckBox("仅看有空闲", this);
    m_sort = new QComboBox(this);
    m_sort->setObjectName("stationSort");
    m_sort->addItems({"距离优先", "空闲优先", "价格优先"});
    m_sort->setAccessibleName("站点排序");
    filterRow->addWidget(m_regionCombo);
    filterRow->addWidget(m_search);
    filterRow->addWidget(m_idleOnly);
    filterRow->addWidget(m_sort);
    filterRow->addStretch(1);
    filterRow->addWidget(refreshBtn);
    layout->addLayout(filterRow);
    m_count = new QLabel(this);
    m_count->setObjectName("pageHint");
    layout->addWidget(m_count);

    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *host = new QWidget(scroll);
    host->setObjectName("stationCardHost");
    m_cards = new QVBoxLayout(host);
    m_cards->setContentsMargins(0, 0, 8, 0);
    m_cards->setSpacing(12);
    m_cards->setAlignment(Qt::AlignTop);
    scroll->setWidget(host);
    layout->addWidget(scroll, 1);
    connect(m_search, &QLineEdit::textChanged, this, &NearbyStationsPage::renderStations);
    connect(m_idleOnly, &QCheckBox::toggled, this, &NearbyStationsPage::renderStations);
    connect(m_sort, qOverload<int>(&QComboBox::currentIndexChanged), this, &NearbyStationsPage::renderStations);

    connect(refreshBtn, &QPushButton::clicked, this, &NearbyStationsPage::refresh);
    connect(m_regionCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &NearbyStationsPage::onRegionChanged);

}

void NearbyStationsPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 小屏优先留出站点与操作空间。
    findChild<QFrame *>("discoveryTerrainHero")->setVisible(height() >= 560);
}

void NearbyStationsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到界面显示完成后再请求, 避免同步网络请求阻塞主窗口首次显示
    QTimer::singleShot(0, this, &NearbyStationsPage::refresh);
}

void NearbyStationsPage::refreshPage()
{
    if (isVisible())
        refresh();
}

void NearbyStationsPage::refresh()
{
    if (m_refreshing) return;
    m_refreshing = true;
    m_summary->setText("正在更新站点状态…");
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationList, QJsonObject{{"lon", m_lon}, {"lat", m_lat}});
    m_refreshing = false;
    m_stations.clear();
    if (!reply.value("ok").toBool()) {
        renderStations();
        m_summary->setText("站点加载失败 · 请检查连接后点击「刷新站点」重试");
        m_summary->setToolTip(reply.value("error").toString());
        return;
    }
    int idle = 0;
    for (const QJsonValue &v : reply.value("stations").toArray()) {
        const StationInfo station = StationInfo::fromJson(v.toObject());
        m_stations.append(station);
        idle += station.idlePiles;
    }
    m_summary->setToolTip(QString());
    m_summary->setText(QString("%1 座充电站    /    %2 个空闲桩    /    状态更新于 %3")
        .arg(m_stations.size()).arg(idle).arg(QTime::currentTime().toString("HH:mm:ss")));
    renderStations();
}

void NearbyStationsPage::renderStations()
{
    while (auto *item = m_cards->takeAt(0)) {
        if (item->widget()) { item->widget()->hide(); item->widget()->deleteLater(); }
        delete item;
    }
    QList<StationInfo> visible;
    const QString query = m_search->text().trimmed();
    for (const auto &s : m_stations) {
        if (m_idleOnly->isChecked() && s.idlePiles <= 0) continue;
        if (!s.name.contains(query, Qt::CaseInsensitive) && !s.address.contains(query, Qt::CaseInsensitive)) continue;
        visible.append(s);
    }
    const int sort = m_sort->currentIndex();
    std::stable_sort(visible.begin(), visible.end(), [sort](const StationInfo &a, const StationInfo &b) {
        if (sort == 1 && a.idlePiles != b.idlePiles) return a.idlePiles > b.idlePiles;
        if (sort == 2 && a.price != b.price) return a.price < b.price;
        return (a.distance < 0 ? 1e10 : a.distance) < (b.distance < 0 ? 1e10 : b.distance);
    });
    m_count->setText(QString("附近充电站 · %1 个结果    查找站点 → 选择电桩 → 导航 / 充电").arg(visible.size()));
    if (visible.isEmpty()) {
        auto *empty = new QLabel("暂无符合条件的站点\n试试更换位置、清空搜索或关闭空闲筛选", this);
        empty->setObjectName("emptyState");
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(120);
        m_cards->addWidget(empty);
    }
    for (const StationInfo &s : visible) {
        auto *card = new QFrame(this);
        card->setObjectName("stationCard");
        auto *body = new QVBoxLayout(card);
        body->setContentsMargins(18, 14, 18, 14);
        body->setSpacing(10);
        auto *top = new QHBoxLayout;
        auto *name = new QLabel(s.name, card);
        name->setTextFormat(Qt::PlainText);
        name->setWordWrap(true);
        name->setObjectName("stationName");
        auto *badge = new QLabel(s.idlePiles > 0 ? QString("空闲 %1 / %2").arg(s.idlePiles).arg(s.totalPiles)
                                                  : (s.totalPiles > 0 ? "暂无空闲" : "暂无电桩"), card);
        badge->setObjectName(s.idlePiles > 0 ? "availableBadge" : "busyBadge");
        top->addWidget(name, 1); top->addWidget(badge);
        body->addLayout(top);
        auto *address = new QLabel(s.address, card);
        address->setTextFormat(Qt::PlainText);
        address->setWordWrap(true);
        address->setObjectName("pageHint");
        body->addWidget(address);
        auto *facts = new QHBoxLayout;
        auto *price = new QLabel(QString("¥ %1 / 度").arg(s.price, 0, 'f', 2), card);
        price->setObjectName("stationPrice");
        facts->addWidget(price);
        facts->addSpacing(16);
        facts->addWidget(new QLabel(s.distance >= 0 ? QString("直线 %1 km").arg(s.distance, 0, 'f', 1) : "距离未知", card));
        facts->addStretch();
        body->addLayout(facts);
        auto *actions = new QHBoxLayout;
        auto *detail = new QPushButton("查看详情", card);
        auto *navigate = new QPushButton("导航", card);
        auto *charge = new QPushButton(s.idlePiles > 0 ? "立即充电" : "预约时段", card);
        charge->setObjectName("primaryBtn");
        charge->setEnabled(s.totalPiles > 0);
        charge->setToolTip("进入本站选择电桩；以最新电桩状态为准");
        for (auto *btn : {detail, navigate, charge}) btn->setCursor(Qt::PointingHandCursor);
        actions->addWidget(detail); actions->addWidget(navigate); actions->addStretch(); actions->addWidget(charge);
        body->addLayout(actions);
        connect(detail, &QPushButton::clicked, this, [this, id = s.id] { showPileDetail(id); });
        connect(navigate, &QPushButton::clicked, this, [this, id = s.id] { emit navigationRequested(id, m_lon, m_lat); });
        connect(charge, &QPushButton::clicked, this, [this, id = s.id] { emit chargeRequested(id); });
        m_cards->addWidget(card);
    }
}

void NearbyStationsPage::onRegionChanged(int index)
{
    if (index < 0 || index >= int(sizeof(kRegions) / sizeof(kRegions[0])))
        return;
    m_lon = kRegions[index].lon;
    m_lat = kRegions[index].lat;
    refresh();
}

void NearbyStationsPage::showPileDetail(int stationId)
{
    StationInfo s;
    bool found = false;
    for (const auto &station : m_stations)
        if (station.id == stationId) { s = station; found = true; break; }
    if (!found) return;

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationPiles, QJsonObject{{"stationId", s.id}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "查询失败", reply.value("error").toString());
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString("站内电桩详情 - %1").arg(s.name));
    dlg.setModal(true);
    dlg.resize(740, 480);
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);

    QLabel *info = new QLabel(
        QString("地址: %1    电价: %2 元/度    电桩: %3 台 / 空闲 %4 台")
            .arg(s.address).arg(s.price, 0, 'f', 2).arg(s.totalPiles).arg(s.idlePiles),
        &dlg);
    info->setObjectName("pageHint");
    info->setWordWrap(true);
    info->setTextFormat(Qt::PlainText);
    layout->addWidget(info);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setObjectName("pileTable");
    table->setProperty("cardTitle", QStringLiteral("站内电桩"));
    table->setProperty("cardHint", QStringLiteral("状态会随服务端刷新；空闲桩可直接进入充电流程"));
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setColumnCount(4);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setMinimumSectionSize(80);
    table->setHorizontalHeaderLabels({ "编号", "类型", "功率(kW)", "状态" });
    layout->addWidget(table, 1);

    const QJsonArray piles = reply.value("piles").toArray();
    table->setRowCount(piles.size());
    for (int i = 0; i < piles.size(); ++i) {
        const PileInfo p = PileInfo::fromJson(piles[i].toObject());
        table->setItem(i, 0, new QTableWidgetItem(p.code));
        table->setItem(i, 1, new QTableWidgetItem(p.type == PileFast ? "快充" : "慢充"));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(p.power, 'f', 1)));
        QString statusText;
        switch (p.status) {
        case PileIdle:  statusText = QStringLiteral("空闲"); break;
        case PileInUse: statusText = QStringLiteral("充电中"); break;
        default:        statusText = QStringLiteral("故障"); break;
        }
        table->setItem(i, 3, new QTableWidgetItem(statusText));
    }
    table->resizeColumnsToContents();

    QPushButton *closeBtn = new QPushButton("关闭", &dlg);
    closeBtn->setObjectName("secondaryBtn");
    auto *actions = new QHBoxLayout;
    auto *navigate = new QPushButton("导航到此站", &dlg);
    auto *charge = new QPushButton("选择电桩 / 预约时段", &dlg);
    charge->setObjectName("primaryBtn");
    charge->setEnabled(!piles.isEmpty());
    actions->addWidget(navigate); actions->addWidget(charge); actions->addStretch(); actions->addWidget(closeBtn);
    layout->addLayout(actions);
    AdminTableCard::decorate(&dlg);
    connect(navigate, &QPushButton::clicked, &dlg, [&] {
        dlg.accept(); emit navigationRequested(s.id, m_lon, m_lat);
    });
    connect(charge, &QPushButton::clicked, &dlg, [&] {
        dlg.accept(); emit chargeRequested(s.id);
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}
