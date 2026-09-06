#include "NearbyStationsPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include "IconFactory.h"
#include <QScrollArea>
#include <QCheckBox>
#include <QFrame>
#include <QDateTime>
#include <algorithm>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
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

// 地址关键词 → 演示坐标。课程项目不在每次输入时调用商业地理编码服务，
// 避免 Key 泄露、额度耗尽和答辩环境网络不稳定。
const RegionCoord kLandmarks[] = {
    { "五道口",    116.3391, 39.9911 },
    { "中关村",    116.3160, 39.9830 },
    { "鸟巢",      116.3956, 39.9926 },
    { "国家体育场", 116.3956, 39.9926 },
    { "水立方",    116.3927, 39.9921 },
    { "国贸",      116.4610, 39.9087 },
    { "望京",      116.4740, 39.9960 },
    { "北京站",    116.4270, 39.9030 },
    { "北京南站",  116.3785, 39.8655 },
    { "大兴机场",  116.4204, 39.5295 },
    { "天安门",    116.3975, 39.9087 },
    { "西单",      116.3740, 39.9130 },
    { "王府井",    116.4110, 39.9160 },
    { "海淀",      116.3100, 39.9600 },
    { "朝阳",      116.4430, 39.9210 },
    { "东城",      116.4160, 39.9160 },
    { "西城",      116.3660, 39.9150 },
    { "丰台",      116.2860, 39.8580 },
    { "大兴",      116.3410, 39.7280 },
    { "通州",      116.6560, 39.9100 },
};
} // namespace

NearbyStationsPage::NearbyStationsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QFrame *hero = new QFrame(this);
    hero->setObjectName("discoveryHero");
    QHBoxLayout *heroRow = new QHBoxLayout(hero);
    heroRow->setContentsMargins(24, 20, 24, 20);
    QVBoxLayout *intro = new QVBoxLayout;
    auto *eyebrow = new QLabel("NEUSOFT  /  CHARGE YOUR JOURNEY", hero);
    eyebrow->setObjectName("heroEyebrow");
    auto *title = new QLabel("下一程，满电出发", hero);
    title->setObjectName("heroTitle");
    auto *subtitle = new QLabel("发现身边好站 · 选桩即充 · 从容出发", hero);
    subtitle->setObjectName("heroSubtitle");
    intro->addWidget(eyebrow); intro->addWidget(title); intro->addWidget(subtitle);
    heroRow->addLayout(intro, 1);
    auto *art = new QLabel(hero);
    art->setObjectName("heroArt");
    const QIcon plug(":/icons/lucide/plug-zap.svg");
    art->setPixmap(plug.isNull() ? IconFactory::icon(IconFactory::IconPile, QColor("#8BF0CE"), 90).pixmap(90, 90)
                                  : plug.pixmap(90, 90));
    heroRow->addWidget(art);
    layout->addWidget(hero);

    m_summary = new QLabel("正在发现附近充电站…", this);
    m_summary->setObjectName("discoverySummary");
    layout->addWidget(m_summary);

    QHBoxLayout *topRow = new QHBoxLayout();
    m_regionCombo = new QComboBox(this);
    m_regionCombo->setObjectName("regionCombo");
    m_regionCombo->setAccessibleName("演示位置区域");
    for (const RegionCoord &r : kRegions)
        m_regionCombo->addItem(QString::fromUtf8(r.name));
    m_addrEdit = new QLineEdit(this);
    m_addrEdit->setObjectName("addrEdit");
    m_addrEdit->setPlaceholderText("输入演示地标：五道口 / 国贸 / 鸟巢");
    m_addrEdit->setAccessibleName("演示地标");
    m_addrEdit->setClearButtonEnabled(true);
    QPushButton *locateBtn = new QPushButton("定位", this);
    QPushButton *refreshBtn = new QPushButton("刷新站点", this);
    refreshBtn->setObjectName("secondaryBtn");
    topRow->addWidget(new QLabel("演示位置", this));
    topRow->addWidget(m_regionCombo);
    topRow->addWidget(m_addrEdit, 1);
    topRow->addWidget(locateBtn);
    topRow->addWidget(refreshBtn);
    layout->addLayout(topRow);

    auto *filters = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("搜索站名或地址");
    m_search->setAccessibleName("搜索站名或地址");
    m_search->setClearButtonEnabled(true);
    m_idleOnly = new QCheckBox("仅看有空闲", this);
    m_sort = new QComboBox(this);
    m_sort->addItems({"距离优先", "空闲优先", "价格优先"});
    m_sort->setAccessibleName("站点排序");
    filters->addWidget(m_search, 1);
    filters->addWidget(m_idleOnly);
    filters->addWidget(m_sort);
    layout->addLayout(filters);
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
    connect(locateBtn, &QPushButton::clicked, this, &NearbyStationsPage::onLocate);
    connect(m_addrEdit, &QLineEdit::returnPressed, this, &NearbyStationsPage::onLocate);
    connect(m_regionCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &NearbyStationsPage::onRegionChanged);

}

void NearbyStationsPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 小屏优先留出站点与操作空间。
    findChild<QFrame *>("discoveryHero")->setVisible(height() >= 560);
}

void NearbyStationsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到界面显示完成后再请求, 避免同步网络请求阻塞主窗口首次显示
    QTimer::singleShot(0, this, &NearbyStationsPage::refresh);
}

void NearbyStationsPage::onLocate()
{
    const QString addr = m_addrEdit->text().trimmed();
    if (addr.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入要定位的地址");
        m_addrEdit->setFocus();
        return;
    }

    bool found = false;
    for (const RegionCoord &l : kLandmarks) {
        if (addr.contains(QString::fromUtf8(l.name))) {
            m_lon = l.lon;
            m_lat = l.lat;
            found = true;
            break;
        }
    }
    if (!found) {
        QMessageBox::warning(this, "定位失败",
                             QString("本地未识别地址：%1\n可使用城区下拉框，或输入：五道口 / 国贸 / 鸟巢 / 北京站 等")
                                 .arg(addr));
        return;
    }

    // 若地址中带有城区名, 只同步下拉框显示；
    // 不触发 onRegionChanged，否则会把精确地标覆盖成城区中心坐标。
    for (int i = 0; i < int(sizeof(kRegions) / sizeof(kRegions[0])); ++i) {
        QString regionName = QString::fromUtf8(kRegions[i].name);
        const int paren = regionName.indexOf('(');
        if (paren > 0)
            regionName = regionName.left(paren);
        if (!regionName.isEmpty() && addr.contains(regionName)) {
            m_regionCombo->blockSignals(true);
            m_regionCombo->setCurrentIndex(i);
            m_regionCombo->blockSignals(false);
            break;
        }
    }

    QMessageBox::information(this, "定位成功",
                             QString("已定位到: %1  (经度 %2, 纬度 %3)")
                                 .arg(addr).arg(m_lon, 0, 'f', 4).arg(m_lat, 0, 'f', 4));
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
        auto *charge = new QPushButton(s.idlePiles > 0 ? "立即充电" : "预约 / 排队", card);
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

    QLabel *section = new QLabel("站内电桩列表", &dlg);
    section->setObjectName("sectionTitle");

    QLabel *info = new QLabel(
        QString("地址: %1    电价: %2 元/度    电桩: %3 台 / 空闲 %4 台")
            .arg(s.address).arg(s.price, 0, 'f', 2).arg(s.totalPiles).arg(s.idlePiles),
        &dlg);
    info->setObjectName("pageHint");
    info->setWordWrap(true);
    info->setTextFormat(Qt::PlainText);
    layout->addWidget(section);
    layout->addWidget(info);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setObjectName("pileTable");
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
        QColor statusColor;
        switch (p.status) {
        case PileIdle:  statusText = "空闲"; statusColor = QColor("#1F9D67"); break;
        case PileInUse: statusText = "充电中"; statusColor = QColor("#B0863F"); break;
        default:        statusText = "故障"; statusColor = QColor("#C5525A"); break;
        }
        auto *statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(QBrush(statusColor));
        table->setItem(i, 3, statusItem);
    }
    table->resizeColumnsToContents();

    QPushButton *closeBtn = new QPushButton("关闭", &dlg);
    closeBtn->setObjectName("secondaryBtn");
    auto *actions = new QHBoxLayout;
    auto *navigate = new QPushButton("导航到此站", &dlg);
    auto *charge = new QPushButton("选择电桩 / 预约排队", &dlg);
    charge->setObjectName("primaryBtn");
    charge->setEnabled(!piles.isEmpty());
    actions->addWidget(navigate); actions->addWidget(charge); actions->addStretch(); actions->addWidget(closeBtn);
    layout->addLayout(actions);
    connect(navigate, &QPushButton::clicked, &dlg, [&] {
        dlg.accept(); emit navigationRequested(s.id, m_lon, m_lat);
    });
    connect(charge, &QPushButton::clicked, &dlg, [&] {
        dlg.accept(); emit chargeRequested(s.id);
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}
