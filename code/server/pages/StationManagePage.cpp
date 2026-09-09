#include "StationManagePage.h"

#include "ChargingEngine.h"
#include "LogDao.h"
#include "PileDao.h"
#include "ServerSession.h"
#include "StationDao.h"
#include "AdminTableCard.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString pileStatusText(int status)
{
    switch (status) {
    case PileIdle:  return QStringLiteral("空闲");
    case PileInUse: return QStringLiteral("充电中");
    case PileFault: return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
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
    m_statusFilter->addItem(QStringLiteral("空闲"), PileIdle);
    m_statusFilter->addItem(QStringLiteral("充电中"), PileInUse);
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

    filterRow->addWidget(m_searchEdit);
    filterRow->addWidget(m_statusFilter);
    filterRow->addStretch(1);
    filterRow->addWidget(searchBtn);
    filterRow->addWidget(refreshBtn);
    actionRow->addWidget(addBtn);
    actionRow->addWidget(m_restartBtn);
    actionRow->addWidget(m_faultBtn);
    actionRow->addStretch();

    auto *stationSectionTitle = new QLabel(QStringLiteral("选择充电站"), this);
    stationSectionTitle->setObjectName("sectionTitle");
    m_stationCardsScroll = new QScrollArea(this);
    m_stationCardsScroll->setObjectName("stationCardsScroll");
    m_stationCardsScroll->setWidgetResizable(false);
    m_stationCardsScroll->setFrameShape(QFrame::NoFrame);
    m_stationCardsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_stationCardsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_stationCardsScroll->setFixedHeight(146);
    m_stationCardsHost = new QWidget(m_stationCardsScroll);
    m_stationCardsHost->setObjectName("stationManageCardHost");
    m_stationCardsHost->setFixedHeight(132);
    m_stationCardsLayout = new QHBoxLayout(m_stationCardsHost);
    m_stationCardsLayout->setContentsMargins(2, 2, 8, 8);
    m_stationCardsLayout->setSpacing(12);
    m_stationCardsLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_stationCardsScroll->setWidget(m_stationCardsHost);

    auto *detailHeader = new QHBoxLayout;
    m_detailTitle = new QLabel(QStringLiteral("点击上方充电站卡片查看电桩"), this);
    m_detailTitle->setObjectName("sectionTitle");
    auto *occupancyCaption = new QLabel(QStringLiteral("本站占用率"), this);
    occupancyCaption->setObjectName("stationOccupancyCaption");
    m_occupancyBar = new QProgressBar(this);
    m_occupancyBar->setObjectName("stationOccupancyBar");
    m_occupancyBar->setRange(0, 100);
    m_occupancyBar->setValue(0);
    m_occupancyBar->setFormat(QStringLiteral("%p%"));
    m_occupancyBar->setMinimumWidth(150);
    detailHeader->addWidget(m_detailTitle, 1);
    detailHeader->addWidget(occupancyCaption);
    detailHeader->addWidget(m_occupancyBar);
    m_pileTable = new QTableWidget(this);
    m_pileTable->setObjectName("pileTable");
    m_pileTable->setProperty("cardTitle", QStringLiteral("本站电桩"));
    m_pileTable->setProperty("cardHint", QStringLiteral("选择电桩后可执行远程重启或故障处理"));
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
    layout->addWidget(title);
    layout->addLayout(filterRow);
    layout->addLayout(actionRow);
    layout->addWidget(stationSectionTitle);
    layout->addWidget(m_stationCardsScroll);
    layout->addLayout(detailHeader);
    layout->addWidget(m_pileTable, 1);

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
    for (const PileInfo &pile : allPiles) {
        pilesByStation[pile.stationId].append(pile);
    }

    while (QLayoutItem *item = m_stationCardsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QList<StationInfo> stations = StationDao::list();
    QList<QPushButton *> visibleCards;
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
        const QList<PileInfo> stationPiles = pilesByStation.value(station.id);
        int idle = 0, charging = 0, fault = 0;
        for (const PileInfo &pile : stationPiles) {
            if (pile.status == PileIdle) ++idle;
            else if (pile.status == PileInUse) ++charging;
            else ++fault;
        }

        auto *card = new QPushButton(m_stationCardsHost);
        card->setObjectName("stationManageCard");
        card->setAccessibleName(station.name);
        card->setCursor(Qt::PointingHandCursor);
        card->setCheckable(true);
        card->setProperty("stationId", station.id);
        card->setProperty("stationName", station.name);
        card->setProperty("stationMatches", stationMatches);
        card->setProperty("microScale", 1.025);
        card->setToolTip(QStringLiteral("点击查看 %1 的电桩详情").arg(station.name));
        auto *body = new QVBoxLayout(card);
        body->setContentsMargins(15, 12, 15, 11);
        body->setSpacing(5);
        auto *name = new QLabel(station.name, card);
        name->setObjectName("stationCardName");
        name->setTextFormat(Qt::PlainText);
        auto *address = new QLabel(station.address, card);
        address->setObjectName("stationCardAddress");
        address->setTextFormat(Qt::PlainText);
        address->setToolTip(station.address);
        auto *summary = new QLabel(QStringLiteral("空闲 %1  ·  充电中 %2  ·  故障 %3")
                                       .arg(idle).arg(charging).arg(fault), card);
        summary->setObjectName("stationCardStats");
        auto *bar = new QProgressBar(card);
        bar->setObjectName("stationCardOccupancy");
        bar->setRange(0, 100);
        bar->setValue(stationPiles.isEmpty() ? 0
                                             : qRound(charging * 100.0 / stationPiles.size()));
        bar->setFormat(QStringLiteral("占用 %p%  ·  共 %1 台").arg(stationPiles.size()));
        for (QWidget *child : QList<QWidget *>{name, address, summary, bar})
            child->setAttribute(Qt::WA_TransparentForMouseEvents);
        body->addWidget(name);
        body->addWidget(address);
        body->addWidget(summary);
        body->addWidget(bar);
        // QPushButton 在安装子布局时会重新计算 sizeHint，固定尺寸必须放在布局
        // 完成之后，否则部分平台会把卡片压回单行按钮高度。
        card->setFixedSize(248, 122);
        m_stationCardsLayout->addWidget(card);
        visibleCards.append(card);
        connect(card, &QPushButton::clicked, this,
                [this, stationId = station.id] { selectStationCard(stationId); });
    }
    m_stationCardsHost->resize(qMax(m_stationCardsScroll->viewport()->width(),
                                    visibleCards.size() * 260), 132);

    QPushButton *cardToSelect = nullptr;
    for (QPushButton *card : visibleCards)
        if (card->property("stationId").toInt() == selectedStationId) {
            cardToSelect = card;
            break;
        }
    if (!cardToSelect && !visibleCards.isEmpty())
        cardToSelect = visibleCards.first();
    m_selectedPileId = selectedPileId;
    if (cardToSelect)
        selectStationCard(cardToSelect->property("stationId").toInt());
    else {
        m_selectedStationId = -1;
        m_selectedPileId = -1;
        m_pileTable->setRowCount(0);
        m_occupancyBar->setValue(0);
        m_occupancyBar->setFormat(QStringLiteral("0%"));
        m_detailTitle->setText(QStringLiteral("未找到符合条件的充电站或电桩"));
        onPileSelected();
    }
}

void StationManagePage::selectStationCard(int stationId)
{
    QPushButton *selectedCard = nullptr;
    for (QPushButton *card : m_stationCardsHost->findChildren<QPushButton *>(
             QStringLiteral("stationManageCard"), Qt::FindDirectChildrenOnly)) {
        const bool selected = card->property("stationId").toInt() == stationId;
        card->setChecked(selected);
        if (selected) selectedCard = card;
    }
    if (!selectedCard) {
        m_selectedStationId = -1;
        m_pileTable->setRowCount(0);
        onPileSelected();
        return;
    }
    m_selectedStationId = stationId;
    loadPileDetail(m_selectedStationId,
                   selectedCard->property("stationName").toString(),
                   selectedCard->property("stationMatches").toBool());
}

void StationManagePage::loadPileDetail(int stationId, const QString &stationName,
                                       bool stationMatches)
{
    const int selectedPileId = m_selectedPileId;
    const QList<PileInfo> piles = PileDao::listByStation(stationId);
    int idle = 0, charging = 0, fault = 0;
    for (const PileInfo &pile : piles) {
        if (pile.status == PileIdle) ++idle;
        else if (pile.status == PileInUse) ++charging;
        else ++fault;
    }
    const int occupancy = piles.isEmpty() ? 0 : qRound(charging * 100.0 / piles.size());
    m_occupancyBar->setValue(occupancy);
    m_occupancyBar->setFormat(QStringLiteral("%1% · %2/%3 充电中")
                                  .arg(occupancy).arg(charging).arg(piles.size()));

    m_pileTable->blockSignals(true);
    m_pileTable->setRowCount(0);
    QHash<QString, QString> currentValues;
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
        auto *powerItem = new QTableWidgetItem(QString::number(pile.power, 'f', 1));
        const QString powerKey = QStringLiteral("%1/power").arg(pile.id);
        currentValues.insert(powerKey, powerItem->text());
        if (m_previousPileValues.contains(powerKey)
            && m_previousPileValues.value(powerKey) != powerItem->text())
            AdminTableCard::markUpdated(powerItem);
        m_pileTable->setItem(row, 2, powerItem);
        auto *statusItem = new QTableWidgetItem(pileStatusText(pile.status));
        const QString statusKey = QStringLiteral("%1/status").arg(pile.id);
        currentValues.insert(statusKey, statusItem->text());
        if (m_previousPileValues.contains(statusKey)
            && m_previousPileValues.value(statusKey) != statusItem->text())
            AdminTableCard::markUpdated(statusItem);
        m_pileTable->setItem(row, 3, statusItem);
        m_pileTable->setItem(row, 4, new QTableWidgetItem(QString::number(pile.totalCount)));
        auto *durationItem = new QTableWidgetItem(
            QString::number(pile.totalDuration / 60.0, 'f', 1));
        const QString durationKey = QStringLiteral("%1/duration").arg(pile.id);
        currentValues.insert(durationKey, durationItem->text());
        if (m_previousPileValues.contains(durationKey)
            && m_previousPileValues.value(durationKey) != durationItem->text())
            AdminTableCard::markUpdated(durationItem);
        m_pileTable->setItem(row, 5, durationItem);
    }
    m_previousPileValues = currentValues;
    m_pileTable->resizeColumnsToContents();
    m_pileTable->blockSignals(false);
    m_detailTitle->setText(QStringLiteral("%1 · 空闲 %2 · 充电中 %3 · 故障 %4 · 当前显示 %5 台")
                               .arg(stationName).arg(idle).arg(charging).arg(fault)
                               .arg(m_pileTable->rowCount()));

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
