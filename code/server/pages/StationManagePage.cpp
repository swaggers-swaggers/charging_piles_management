#include "StationManagePage.h"

#include "ChargingEngine.h"
#include "LogDao.h"
#include "PileDao.h"
#include "ServerSession.h"
#include "StationDao.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString pileStatusText(int status)
{
    switch (status) {
    case PileIdle:  return QStringLiteral("闲置");
    case PileInUse: return QStringLiteral("使用中");
    case PileFault: return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}

QColor pileStatusColor(int status)
{
    switch (status) {
    case PileInUse: return QColor("#B0863F");
    case PileFault: return QColor("#C5525A");
    default:        return QColor("#1F9D67");
    }
}

QString stationStatusText(int total, int inUse, int fault)
{
    if (inUse > 0)
        return QStringLiteral("使用中");
    if (total > 0 && fault == total)
        return QStringLiteral("故障");
    if (fault > 0)
        return QStringLiteral("部分故障");
    return QStringLiteral("闲置");
}
} // namespace

StationManagePage::StationManagePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("充电站与充电桩管理"), this);
    title->setObjectName("pageTitle");

    auto *filterRow = new QHBoxLayout;
    auto *actionRow = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("stationPileSearch");
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索站点名称、地址或电桩编号"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(240);
    m_statusFilter = new QComboBox(this);
    m_statusFilter->setObjectName("pileStatusFilter");
    m_statusFilter->addItem(QStringLiteral("全部状态"), -1);
    m_statusFilter->addItem(QStringLiteral("闲置"), PileIdle);
    m_statusFilter->addItem(QStringLiteral("使用中"), PileInUse);
    m_statusFilter->addItem(QStringLiteral("故障"), PileFault);
    auto *searchBtn = new QPushButton(QStringLiteral("搜索"), this);
    searchBtn->setObjectName("searchButton");
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn->setObjectName("refreshButton");
    auto *addBtn = new QPushButton(QStringLiteral("新增电站"), this);
    addBtn->setObjectName("addButton");
    m_restartBtn = new QPushButton(QStringLiteral("远程重启"), this);
    m_restartBtn->setObjectName("restartButton");
    m_restartBtn->setEnabled(false);
    m_faultBtn = new QPushButton(QStringLiteral("设为故障"), this);
    m_faultBtn->setObjectName("faultButton");
    m_faultBtn->setEnabled(false);

    filterRow->addWidget(m_searchEdit, 1);
    filterRow->addWidget(m_statusFilter);
    filterRow->addWidget(searchBtn);
    filterRow->addWidget(refreshBtn);
    actionRow->addWidget(addBtn);
    actionRow->addWidget(m_restartBtn);
    actionRow->addWidget(m_faultBtn);
    actionRow->addStretch();

    m_stationTable = new QTableWidget(this);
    m_stationTable->setObjectName("stationTable");
    m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stationTable->setAlternatingRowColors(true);
    m_stationTable->verticalHeader()->setVisible(false);
    m_stationTable->setColumnCount(10);
    m_stationTable->horizontalHeader()->setStretchLastSection(true);
    m_stationTable->horizontalHeader()->setMinimumSectionSize(70);
    m_stationTable->setHorizontalHeaderLabels(
        { QStringLiteral("ID"), QStringLiteral("站名"), QStringLiteral("详细地址"),
          QStringLiteral("电价"), QStringLiteral("总桩数"), QStringLiteral("闲置"),
          QStringLiteral("使用中"), QStringLiteral("故障"), QStringLiteral("站点状态"),
          QStringLiteral("在线率") });

    auto *detailPanel = new QWidget(this);
    detailPanel->setObjectName("detailPanel");
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(14, 12, 14, 12);
    detailLayout->setSpacing(8);
    m_detailTitle = new QLabel(QStringLiteral("电桩明细（选择上方充电站）"), detailPanel);
    m_detailTitle->setObjectName("sectionTitle");
    m_pileTable = new QTableWidget(detailPanel);
    m_pileTable->setObjectName("pileTable");
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pileTable->setAlternatingRowColors(true);
    m_pileTable->verticalHeader()->setVisible(false);
    m_pileTable->setColumnCount(6);
    m_pileTable->horizontalHeader()->setStretchLastSection(true);
    m_pileTable->horizontalHeader()->setMinimumSectionSize(80);
    m_pileTable->setHorizontalHeaderLabels(
        { QStringLiteral("电桩编号"), QStringLiteral("类型"), QStringLiteral("功率(kW)"),
          QStringLiteral("状态"), QStringLiteral("累计次数"), QStringLiteral("累计时长(小时)") });
    detailLayout->addWidget(m_detailTitle);
    detailLayout->addWidget(m_pileTable, 1);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("stationPileSplitter");
    splitter->addWidget(m_stationTable);
    splitter->addWidget(detailPanel);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({660, 440});
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(title);
    layout->addLayout(filterRow);
    layout->addLayout(actionRow);
    layout->addWidget(splitter, 1);

    connect(searchBtn, &QPushButton::clicked, this, &StationManagePage::refresh);
    connect(refreshBtn, &QPushButton::clicked, this, &StationManagePage::refresh);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &StationManagePage::refresh);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) refresh();
    });
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StationManagePage::refresh);
    connect(addBtn, &QPushButton::clicked, this, &StationManagePage::onAddStation);
    connect(m_restartBtn, &QPushButton::clicked, this, &StationManagePage::onRestartPile);
    connect(m_faultBtn, &QPushButton::clicked, this, &StationManagePage::onTogglePileFault);
    connect(m_stationTable, &QTableWidget::itemSelectionChanged,
            this, &StationManagePage::onStationSelected);
    connect(m_pileTable, &QTableWidget::itemSelectionChanged,
            this, &StationManagePage::onPileSelected);
    refresh();
}

void StationManagePage::refreshPage()
{
    refresh();
}

bool StationManagePage::pileMatchesFilter(const PileInfo &pile, bool stationMatches) const
{
    const int status = m_statusFilter->currentData().toInt();
    if (status >= 0 && pile.status != status)
        return false;
    const QString query = m_searchEdit->text().trimmed();
    return query.isEmpty() || stationMatches
           || pile.code.contains(query, Qt::CaseInsensitive);
}

void StationManagePage::refresh()
{
    const int selectedStationId = m_selectedStationId;
    const int selectedPileId = m_selectedPileId;
    const QString query = m_searchEdit->text().trimmed();
    const int status = m_statusFilter->currentData().toInt();
    const QList<PileInfo> allPiles = PileDao::listAll();

    QHash<int, QList<PileInfo>> pilesByStation;
    QHash<int, int> idleByStation;
    QHash<int, int> inUseByStation;
    QHash<int, int> faultByStation;
    for (const PileInfo &pile : allPiles) {
        pilesByStation[pile.stationId].append(pile);
        if (pile.status == PileIdle) ++idleByStation[pile.stationId];
        else if (pile.status == PileInUse) ++inUseByStation[pile.stationId];
        else if (pile.status == PileFault) ++faultByStation[pile.stationId];
    }

    const QList<StationInfo> stations = StationDao::list();
    m_stationTable->blockSignals(true);
    m_stationTable->setRowCount(0);
    for (const StationInfo &station : stations) {
        const bool stationMatches = query.isEmpty()
            || station.name.contains(query, Qt::CaseInsensitive)
            || station.address.contains(query, Qt::CaseInsensitive);
        bool hasMatchingPile = false;
        for (const PileInfo &pile : pilesByStation.value(station.id)) {
            if (pileMatchesFilter(pile, stationMatches)) {
                hasMatchingPile = true;
                break;
            }
        }
        if ((!stationMatches && !hasMatchingPile) || (status >= 0 && !hasMatchingPile))
            continue;

        const int row = m_stationTable->rowCount();
        m_stationTable->insertRow(row);
        auto *idItem = new QTableWidgetItem(QString::number(station.id));
        idItem->setData(Qt::UserRole, station.id);
        m_stationTable->setItem(row, 0, idItem);
        m_stationTable->setItem(row, 1, new QTableWidgetItem(station.name));
        m_stationTable->setItem(row, 2, new QTableWidgetItem(station.address));
        m_stationTable->setItem(row, 3, new QTableWidgetItem(QString::number(station.price, 'f', 2)));
        const int idle = idleByStation.value(station.id);
        const int inUse = inUseByStation.value(station.id);
        const int fault = faultByStation.value(station.id);
        const int total = pilesByStation.value(station.id).size();
        m_stationTable->setItem(row, 4, new QTableWidgetItem(QString::number(total)));
        m_stationTable->setItem(row, 5, new QTableWidgetItem(QString::number(idle)));
        m_stationTable->setItem(row, 6, new QTableWidgetItem(QString::number(inUse)));
        m_stationTable->setItem(row, 7, new QTableWidgetItem(QString::number(fault)));
        auto *statusItem = new QTableWidgetItem(stationStatusText(total, inUse, fault));
        statusItem->setForeground(QBrush(inUse > 0 ? QColor("#B0863F")
                                        : fault > 0 ? QColor("#C5525A") : QColor("#1F9D67")));
        m_stationTable->setItem(row, 8, statusItem);
        const double rate = total > 0 ? (total - fault) * 100.0 / total : 100.0;
        m_stationTable->setItem(row, 9,
                                new QTableWidgetItem(QString::number(rate, 'f', 1) + "%"));
    }
    m_stationTable->resizeColumnsToContents();
    m_stationTable->blockSignals(false);

    int rowToSelect = -1;
    for (int row = 0; row < m_stationTable->rowCount(); ++row) {
        if (m_stationTable->item(row, 0)->data(Qt::UserRole).toInt() == selectedStationId) {
            rowToSelect = row;
            break;
        }
    }
    if (rowToSelect < 0 && m_stationTable->rowCount() > 0)
        rowToSelect = 0;
    m_selectedPileId = selectedPileId;
    if (rowToSelect >= 0)
        m_stationTable->selectRow(rowToSelect);
    else {
        m_selectedStationId = -1;
        m_selectedPileId = -1;
        m_pileTable->setRowCount(0);
        m_detailTitle->setText(QStringLiteral("未找到符合条件的充电站或电桩"));
        onPileSelected();
    }
}

void StationManagePage::onStationSelected()
{
    const QList<QTableWidgetItem *> selected = m_stationTable->selectedItems();
    if (selected.isEmpty()) {
        m_selectedStationId = -1;
        m_pileTable->setRowCount(0);
        onPileSelected();
        return;
    }
    const int row = selected.first()->row();
    m_selectedStationId = m_stationTable->item(row, 0)->data(Qt::UserRole).toInt();
    loadPileDetail(m_selectedStationId, m_stationTable->item(row, 1)->text());
}

void StationManagePage::loadPileDetail(int stationId, const QString &stationName)
{
    const int selectedPileId = m_selectedPileId;
    const QString query = m_searchEdit->text().trimmed();
    bool stationMatches = query.isEmpty() || stationName.contains(query, Qt::CaseInsensitive);
    for (int row = 0; row < m_stationTable->rowCount() && !stationMatches; ++row) {
        if (m_stationTable->item(row, 0)->data(Qt::UserRole).toInt() == stationId)
            stationMatches = m_stationTable->item(row, 2)->text().contains(query, Qt::CaseInsensitive);
    }

    const QList<PileInfo> piles = PileDao::listByStation(stationId);
    m_pileTable->blockSignals(true);
    m_pileTable->setRowCount(0);
    for (const PileInfo &pile : piles) {
        if (!pileMatchesFilter(pile, stationMatches))
            continue;
        const int row = m_pileTable->rowCount();
        m_pileTable->insertRow(row);
        auto *codeItem = new QTableWidgetItem(pile.code);
        codeItem->setData(Qt::UserRole, pile.id);
        codeItem->setData(Qt::UserRole + 1, pile.status);
        m_pileTable->setItem(row, 0, codeItem);
        m_pileTable->setItem(row, 1,
                             new QTableWidgetItem(pile.type == PileFast ? QStringLiteral("快充")
                                                                        : QStringLiteral("慢充")));
        m_pileTable->setItem(row, 2, new QTableWidgetItem(QString::number(pile.power, 'f', 1)));
        auto *statusItem = new QTableWidgetItem(pileStatusText(pile.status));
        statusItem->setForeground(QBrush(pileStatusColor(pile.status)));
        m_pileTable->setItem(row, 3, statusItem);
        m_pileTable->setItem(row, 4, new QTableWidgetItem(QString::number(pile.totalCount)));
        m_pileTable->setItem(row, 5,
                             new QTableWidgetItem(QString::number(pile.totalDuration / 60.0, 'f', 1)));
    }
    m_pileTable->resizeColumnsToContents();
    m_pileTable->blockSignals(false);
    m_detailTitle->setText(QStringLiteral("%1 · %2 个符合条件的电桩")
                               .arg(stationName).arg(m_pileTable->rowCount()));

    int rowToSelect = -1;
    for (int row = 0; row < m_pileTable->rowCount(); ++row) {
        if (m_pileTable->item(row, 0)->data(Qt::UserRole).toInt() == selectedPileId) {
            rowToSelect = row;
            break;
        }
    }
    if (rowToSelect < 0 && m_pileTable->rowCount() > 0)
        rowToSelect = 0;
    if (rowToSelect >= 0)
        m_pileTable->selectRow(rowToSelect);
    else
        onPileSelected();
}

void StationManagePage::onPileSelected()
{
    const QList<QTableWidgetItem *> selected = m_pileTable->selectedItems();
    if (selected.isEmpty()) {
        m_selectedPileId = -1;
        m_selectedPileStatus = -1;
        m_selectedPileCode.clear();
        m_restartBtn->setEnabled(false);
        m_faultBtn->setEnabled(false);
        m_faultBtn->setText(QStringLiteral("设为故障"));
        return;
    }
    const QTableWidgetItem *item = m_pileTable->item(selected.first()->row(), 0);
    m_selectedPileId = item->data(Qt::UserRole).toInt();
    m_selectedPileStatus = item->data(Qt::UserRole + 1).toInt();
    m_selectedPileCode = item->text();
    m_restartBtn->setEnabled(m_selectedPileStatus != PileInUse);
    m_faultBtn->setEnabled(true);
    m_faultBtn->setText(m_selectedPileStatus == PileFault
                            ? QStringLiteral("恢复正常") : QStringLiteral("设为故障"));
}

void StationManagePage::onRestartPile()
{
    if (m_selectedPileId < 0 || m_selectedPileStatus == PileInUse)
        return;
    QString error;
    if (!PileDao::restart(m_selectedPileId, &error)) {
        QMessageBox::warning(this, QStringLiteral("重启失败"), error);
        return;
    }
    LogDao::record(ServerSession::instance().adminName, QStringLiteral("远程重启充电桩"),
                   QStringLiteral("电桩 %1").arg(m_selectedPileCode));
    QMessageBox::information(this, QStringLiteral("重启完成"),
                             QStringLiteral("电桩 %1 已恢复为空闲状态").arg(m_selectedPileCode));
    refresh();
}

void StationManagePage::onTogglePileFault()
{
    if (m_selectedPileId < 0)
        return;
    const bool recover = m_selectedPileStatus == PileFault;
    const QString message = recover
        ? QStringLiteral("确定将电桩 %1 恢复为正常闲置状态吗？").arg(m_selectedPileCode)
        : m_selectedPileStatus == PileInUse
            ? QStringLiteral("电桩 %1 正在充电，设为故障会自动中断并结算当前订单。是否继续？")
                  .arg(m_selectedPileCode)
            : QStringLiteral("确定将电桩 %1 设置为故障吗？").arg(m_selectedPileCode);
    if (QMessageBox::question(this, recover ? QStringLiteral("恢复电桩")
                                            : QStringLiteral("设置故障"), message)
        != QMessageBox::Yes)
        return;

    const int oldStatus = m_selectedPileStatus;
    const int newStatus = recover ? PileIdle : PileFault;
    QString error;
    if (!PileDao::setStatus(m_selectedPileId, newStatus, &error)) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), error);
        return;
    }
    LogDao::record(ServerSession::instance().adminName,
                   recover ? QStringLiteral("恢复充电桩") : QStringLiteral("设置充电桩故障"),
                   QStringLiteral("电桩 %1").arg(m_selectedPileCode));
    refresh();
    if (!recover && oldStatus == PileInUse)
        QMetaObject::invokeMethod(&ChargingEngine::instance(), "onTick", Qt::QueuedConnection);
}

void StationManagePage::onAddStation()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增电站"));
    dlg.setFixedWidth(380);
    auto *form = new QFormLayout(&dlg);
    form->setSpacing(12);

    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(QStringLiteral("请输入站名"));
    auto *addrEdit = new QLineEdit(&dlg);
    addrEdit->setPlaceholderText(QStringLiteral("请输入详细地址"));
    auto *lonSpin = new QDoubleSpinBox(&dlg);
    lonSpin->setRange(0.0, 180.0);
    lonSpin->setDecimals(4);
    lonSpin->setSingleStep(0.001);
    lonSpin->setValue(123.4500);
    auto *latSpin = new QDoubleSpinBox(&dlg);
    latSpin->setRange(0.0, 90.0);
    latSpin->setDecimals(4);
    latSpin->setSingleStep(0.001);
    latSpin->setValue(41.7000);
    auto *priceSpin = new QDoubleSpinBox(&dlg);
    priceSpin->setRange(0.1, 5.0);
    priceSpin->setDecimals(2);
    priceSpin->setSingleStep(0.05);
    priceSpin->setValue(1.00);
    priceSpin->setSuffix(QStringLiteral(" 元/度"));
    auto *countSpin = new QSpinBox(&dlg);
    countSpin->setRange(1, 50);
    countSpin->setValue(6);

    form->addRow(QStringLiteral("站名"), nameEdit);
    form->addRow(QStringLiteral("详细地址"), addrEdit);
    form->addRow(QStringLiteral("经度"), lonSpin);
    form->addRow(QStringLiteral("纬度"), latSpin);
    form->addRow(QStringLiteral("电价"), priceSpin);
    form->addRow(QStringLiteral("电桩数量"), countSpin);

    auto *buttonRow = new QHBoxLayout;
    auto *okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
    okBtn->setObjectName("primaryBtn");
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    cancelBtn->setObjectName("secondaryBtn");
    buttonRow->addStretch();
    buttonRow->addWidget(okBtn);
    buttonRow->addWidget(cancelBtn);
    form->addRow(buttonRow);

    connect(okBtn, &QPushButton::clicked, &dlg, [&dlg, nameEdit, addrEdit] {
        if (nameEdit->text().trimmed().isEmpty() || addrEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("提示"),
                                 QStringLiteral("站名和详细地址不能为空"));
            return;
        }
        dlg.accept();
    });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    StationInfo station;
    station.name = nameEdit->text().trimmed();
    station.address = addrEdit->text().trimmed();
    station.longitude = lonSpin->value();
    station.latitude = latSpin->value();
    station.price = priceSpin->value();
    QString error;
    if (!StationDao::add(&station, countSpin->value(), &error)) {
        QMessageBox::warning(this, QStringLiteral("新增失败"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("充电站 %1 新增成功，已生成 %2 个电桩")
                                 .arg(station.name).arg(countSpin->value()));
    m_selectedStationId = station.id;
    refresh();
}
