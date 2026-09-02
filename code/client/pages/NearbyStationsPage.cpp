#include "NearbyStationsPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// 模拟GPS: 区域 → 固定经纬度(沈阳市主要城区)
struct RegionCoord {
    const char *name;
    double lon;
    double lat;
};
const RegionCoord kRegions[] = {
    { "浑南区(默认)",  123.4500, 41.7000 },
    { "和平区",        123.4200, 41.7800 },
    { "沈河区",        123.4500, 41.7900 },
    { "铁西区",        123.3600, 41.7800 },
    { "大东区",        123.4700, 41.8100 },
    { "皇姑区",        123.4100, 41.8200 },
};
} // namespace

NearbyStationsPage::NearbyStationsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("附近充电站", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *topRow = new QHBoxLayout();
    QLabel *regionLabel = new QLabel("当前位置:", this);
    m_regionCombo = new QComboBox(this);
    for (const RegionCoord &r : kRegions)
        m_regionCombo->addItem(QString::fromUtf8(r.name));
    QPushButton *refreshBtn = new QPushButton("查询", this);
    topRow->addWidget(regionLabel);
    topRow->addWidget(m_regionCombo);
    topRow->addWidget(refreshBtn);
    topRow->addStretch();

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        { "站名", "地址", "电价(元/度)", "电桩总数", "空闲", "距离" });

    layout->addWidget(title);
    layout->addLayout(topRow);
    layout->addWidget(m_table, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &NearbyStationsPage::refresh);
    connect(m_regionCombo, &QComboBox::currentIndexChanged, this, &NearbyStationsPage::refresh);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &NearbyStationsPage::showPileDetail);
}

void NearbyStationsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到界面显示完成后再请求, 避免同步网络请求阻塞主窗口首次显示
    QTimer::singleShot(0, this, &NearbyStationsPage::refresh);
}

void NearbyStationsPage::refresh()
{
    const RegionCoord &region = kRegions[m_regionCombo->currentIndex()];
    m_lon = region.lon;
    m_lat = region.lat;

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

    m_table->setRowCount(m_stations.size());
    for (int i = 0; i < m_stations.size(); ++i) {
        const StationInfo &s = m_stations[i];
        auto *nameItem = new QTableWidgetItem(s.name);
        nameItem->setData(Qt::UserRole, s.id);
        nameItem->setToolTip("双击查看站内电桩详情");
        m_table->setItem(i, 0, nameItem);
        m_table->setItem(i, 1, new QTableWidgetItem(s.address));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(s.price, 'f', 2)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(s.totalPiles)));
        auto *idleItem = new QTableWidgetItem(QString::number(s.idlePiles));
        idleItem->setForeground(QBrush(s.idlePiles > 0 ? QColor("#1D976C") : QColor("#E5484D")));
        m_table->setItem(i, 4, idleItem);
        m_table->setItem(i, 5, new QTableWidgetItem(QString::number(s.distance, 'f', 1) + " km"));

        // 预计空闲率(负荷预测): 有数据时附在地址下方提示
        if (s.predictIdle >= 0)
            nameItem->setToolTip(QString("双击查看电桩详情\n预计1小时后空闲率: %1%")
                                     .arg(qRound(s.predictIdle * 100)));
    }
    m_table->resizeColumnsToContents();
}

void NearbyStationsPage::onStationSelected()
{
    // 预留: 单击选中时的处理(详情通过双击打开)
}

void NearbyStationsPage::showPileDetail()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_stations.size())
        return;
    const StationInfo &s = m_stations[row];

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationPiles, QJsonObject{{"stationId", s.id}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "查询失败", reply.value("error").toString());
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString("站内电桩详情 - %1").arg(s.name));
    dlg.resize(520, 400);
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QLabel *info = new QLabel(
        QString("地址: %1    电价: %2 元/度    电桩: %3 台 / 空闲 %4 台")
            .arg(s.address).arg(s.price, 0, 'f', 2).arg(s.totalPiles).arg(s.idlePiles),
        &dlg);
    layout->addWidget(info);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->setColumnCount(4);
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
        case PileIdle:  statusText = "空闲"; statusColor = QColor("#1D976C"); break;
        case PileInUse: statusText = "充电中"; statusColor = QColor("#2F80ED"); break;
        default:        statusText = "故障"; statusColor = QColor("#E5484D"); break;
        }
        auto *statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(QBrush(statusColor));
        table->setItem(i, 3, statusItem);
    }
    table->resizeColumnsToContents();

    QPushButton *closeBtn = new QPushButton("关闭", &dlg);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}
