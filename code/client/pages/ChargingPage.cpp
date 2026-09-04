#include "ChargingPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack);

    // ---------- 选择视图 ----------
    m_selectView = new QWidget(this);
    QVBoxLayout *selLayout = new QVBoxLayout(m_selectView);
    selLayout->setContentsMargins(24, 20, 24, 24);
    selLayout->setSpacing(16);

    QLabel *selTitle = new QLabel("电动汽车充电", m_selectView);
    selTitle->setObjectName("pageTitle");

    QLabel *hint = new QLabel("选择充电站后, 在下方电桩列表中双击\"空闲\"状态的电桩开始充电", m_selectView);
    hint->setObjectName("pageHint");

    QHBoxLayout *stationRow = new QHBoxLayout();
    QLabel *stationLabel = new QLabel("充电站:", m_selectView);
    m_stationCombo = new QComboBox(m_selectView);
    m_stationCombo->setObjectName("stationCombo");
    m_stationCombo->setMinimumWidth(260);
    QPushButton *refreshBtn = new QPushButton("刷新", m_selectView);
    refreshBtn->setObjectName("searchButton");
    stationRow->addWidget(stationLabel);
    stationRow->addWidget(m_stationCombo);
    stationRow->addWidget(refreshBtn);
    stationRow->addStretch();

    m_pileTable = new QTableWidget(m_selectView);
    m_pileTable->setObjectName("pileTable");
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pileTable->verticalHeader()->setVisible(false);
    m_pileTable->setColumnCount(4);
    m_pileTable->setHorizontalHeaderLabels({ "编号", "类型", "功率(kW)", "状态" });

    selLayout->addWidget(selTitle);
    selLayout->addWidget(hint);
    selLayout->addLayout(stationRow);
    selLayout->addWidget(m_pileTable, 1);

    // ---------- 充电视图 ----------
    m_chargingView = new QWidget(this);
    QVBoxLayout *chgLayout = new QVBoxLayout(m_chargingView);
    chgLayout->setContentsMargins(24, 20, 24, 24);
    chgLayout->setSpacing(16);

    QLabel *chgTitle = new QLabel("充电中", m_chargingView);
    chgTitle->setObjectName("pageTitle");

    m_orderLabel = new QLabel(m_chargingView);
    m_orderLabel->setObjectName("orderLabel");
    m_energyLabel = new QLabel(m_chargingView);
    m_energyLabel->setObjectName("chargingValue");
    m_amountLabel = new QLabel(m_chargingView);
    m_amountLabel->setObjectName("chargingValue");
    m_minutesLabel = new QLabel(m_chargingView);
    m_minutesLabel->setObjectName("minutesLabel");

    QLabel *tip = new QLabel("充电进度每几秒自动刷新(模拟计费), 点击下方按钮结束充电并结算", m_chargingView);
    tip->setObjectName("pageHint");

    QPushButton *stopBtn = new QPushButton("结束充电并结算", m_chargingView);
    stopBtn->setObjectName("settleBtn");

    chgLayout->addWidget(chgTitle);
    chgLayout->addWidget(m_orderLabel);
    chgLayout->addSpacing(10);
    chgLayout->addWidget(m_energyLabel);
    chgLayout->addWidget(m_amountLabel);
    chgLayout->addWidget(m_minutesLabel);
    chgLayout->addSpacing(10);
    chgLayout->addWidget(tip);
    chgLayout->addStretch();
    chgLayout->addWidget(stopBtn);

    m_stack->addWidget(m_selectView);
    m_stack->addWidget(m_chargingView);

    connect(refreshBtn, &QPushButton::clicked, this, &ChargingPage::refreshStations);
    connect(m_stationCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ChargingPage::onStationPicked);
    connect(m_pileTable, &QTableWidget::cellDoubleClicked, this, &ChargingPage::onStartCharge);
    connect(stopBtn, &QPushButton::clicked, this, &ChargingPage::onStopCharge);
    connect(&TcpClient::instance(), &TcpClient::pushReceived,
            this, &ChargingPage::onPushReceived);
}

void ChargingPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // 充电前检查: 存在未完成订单 → 强制进入结算视图(说明书要求)
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUnfinishedOrder,
        QJsonObject{{"userId", ClientSession::instance().userId}});
    if (!reply.value("ok").toBool()) {
        // 查询失败(如服务端暂不可达)时不能留白, 回退到充电站选择视图并提示
        m_hasOrder = false;
        enterSelectView();
        refreshStations();
        QMessageBox::warning(this, "提示", reply.value("error").toString());
        return;
    }

    if (reply.value("hasOrder").toBool()) {
        m_currentOrder = OrderInfo::fromJson(reply.value("order").toObject());
        m_hasOrder = true;
        enterChargingView(m_currentOrder);
        QMessageBox::information(this, "提示",
                                 "您有未完成的充电订单, 请先结算!\n已为您跳转至订单结算页面。");
    } else {
        m_hasOrder = false;
        enterSelectView();
        refreshStations();
    }
}

void ChargingPage::refreshStations()
{
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationList, QJsonObject{{"lon", 123.45}, {"lat", 41.70}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "查询失败", reply.value("error").toString());
        return;
    }

    m_stations.clear();
    const QJsonArray arr = reply.value("stations").toArray();
    for (const QJsonValue &v : arr)
        m_stations.append(StationInfo::fromJson(v.toObject()));

    m_stationCombo->blockSignals(true);
    m_stationCombo->clear();
    for (const StationInfo &s : m_stations)
        m_stationCombo->addItem(QString("%1 (空闲 %2/%3)")
                                     .arg(s.name).arg(s.idlePiles).arg(s.totalPiles));
    m_stationCombo->blockSignals(false);

    onStationPicked();
}

void ChargingPage::onStationPicked()
{
    const int idx = m_stationCombo->currentIndex();
    m_pileTable->setRowCount(0);
    if (idx < 0 || idx >= m_stations.size())
        return;
    const StationInfo &s = m_stations[idx];

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationPiles, QJsonObject{{"stationId", s.id}});
    if (!reply.value("ok").toBool())
        return;

    const QJsonArray piles = reply.value("piles").toArray();
    m_pileTable->setRowCount(piles.size());
    for (int i = 0; i < piles.size(); ++i) {
        const PileInfo p = PileInfo::fromJson(piles[i].toObject());
        auto *codeItem = new QTableWidgetItem(p.code);
        codeItem->setData(Qt::UserRole, p.id);
        codeItem->setData(Qt::UserRole + 1, p.status);
        codeItem->setToolTip(p.status == PileIdle ? "双击开始充电" : "该桩当前不可用");
        m_pileTable->setItem(i, 0, codeItem);
        m_pileTable->setItem(i, 1, new QTableWidgetItem(p.type == PileFast ? "快充" : "慢充"));
        m_pileTable->setItem(i, 2, new QTableWidgetItem(QString::number(p.power, 'f', 1)));
        QString statusText;
        QColor statusColor;
        switch (p.status) {
        case PileIdle:  statusText = "空闲"; statusColor = QColor("#1F9D67"); break;
        case PileInUse: statusText = "充电中"; statusColor = QColor("#B0863F"); break;
        default:        statusText = "故障"; statusColor = QColor("#C5525A"); break;
        }
        auto *statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(QBrush(statusColor));
        m_pileTable->setItem(i, 3, statusItem);
    }
    m_pileTable->resizeColumnsToContents();
}

void ChargingPage::onStartCharge()
{
    const int row = m_pileTable->currentRow();
    if (row < 0)
        return;
    const QTableWidgetItem *codeItem = m_pileTable->item(row, 0);
    const int pileId = codeItem->data(Qt::UserRole).toInt();
    const int status = codeItem->data(Qt::UserRole + 1).toInt();

    if (status != PileIdle) {
        QMessageBox::warning(this, "提示", "请选择\"空闲\"状态的电桩");
        return;
    }

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStartCharge,
        QJsonObject{{"userId", ClientSession::instance().userId},
                    {"pileId", pileId}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "无法开始充电", reply.value("error").toString());
        return;
    }

    m_currentOrder = OrderInfo::fromJson(reply.value("order").toObject());
    m_hasOrder = true;
    enterChargingView(m_currentOrder);
}

void ChargingPage::onStopCharge()
{
    if (!m_hasOrder)
        return;
    if (QMessageBox::question(this, "结算确认",
                              QString("确定结束充电并结算订单 #%1 吗?").arg(m_currentOrder.id))
        != QMessageBox::Yes)
        return;

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStopCharge,
        QJsonObject{{"userId", ClientSession::instance().userId},
                    {"orderId", m_currentOrder.id}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "结算失败", reply.value("error").toString());
        return;
    }
    showSettlement(reply);
}

void ChargingPage::onPushReceived(const QJsonObject &msg)
{
    if (msg.value("type").toInt() != Protocol::PushOrderProgress)
        return;
    if (msg.value("orderId").toInt() != m_currentOrder.id || !m_hasOrder)
        return;

    m_currentOrder.energy = msg.value("energy").toDouble();
    m_currentOrder.amount = msg.value("amount").toDouble();
    m_energyLabel->setText(QString("已充电: %1 度").arg(m_currentOrder.energy, 0, 'f', 2));
    m_amountLabel->setText(QString("当前费用: %1 元").arg(m_currentOrder.amount, 0, 'f', 2));
    m_minutesLabel->setText(QString("充电时长: %1 分钟(模拟)").arg(msg.value("minutes").toInt()));
}

void ChargingPage::enterChargingView(const OrderInfo &order)
{
    m_currentOrder = order;
    m_orderLabel->setText(QString("订单 #%1    %2    电桩 %3")
                              .arg(order.id)
                              .arg(order.stationName)
                              .arg(order.pileCode));
    m_energyLabel->setText(QString("已充电: %1 度").arg(order.energy, 0, 'f', 2));
    m_amountLabel->setText(QString("当前费用: %1 元").arg(order.amount, 0, 'f', 2));
    m_minutesLabel->setText("等待服务端推送充电进度...");

    m_stack->setCurrentIndex(1);   // 统一由 QStackedWidget 切换
}

void ChargingPage::enterSelectView()
{
    m_stack->setCurrentIndex(0);
}

void ChargingPage::showSettlement(const QJsonObject &reply)
{
    const OrderInfo order = OrderInfo::fromJson(reply.value("order").toObject());
    ClientSession::instance().balance = reply.value("balance").toDouble();
    m_hasOrder = false;

    QMessageBox::information(
        this, "结算成功",
        QString("订单 #%1 已完成!\n\n"
                "充电站: %2\n电桩: %3\n充电电量: %4 度\n消费金额: %5 元\n\n当前余额: %6 元")
            .arg(order.id)
            .arg(order.stationName)
            .arg(order.pileCode)
            .arg(order.energy, 0, 'f', 2)
            .arg(order.amount, 0, 'f', 2)
            .arg(ClientSession::instance().balance, 0, 'f', 2));

    enterSelectView();
    refreshStations();
}
