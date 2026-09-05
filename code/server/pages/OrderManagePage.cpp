#include "OrderManagePage.h"

#include "ChargingEngine.h"
#include "LogDao.h"
#include "ServerSession.h"
#include "dao/OrderDao.h"
#include "dao/ReservationDao.h"
#include "types.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
QString orderStatusText(int s)
{
    switch (s) {
    case OrderCharging:  return QStringLiteral("充电中");
    case OrderFinished:  return QStringLiteral("已完成");
    case OrderWaiting:   return QStringLiteral("排队中");
    case OrderCancelled: return QStringLiteral("已取消");
    case OrderAbnormal:  return QStringLiteral("异常中断");
    default:             return QStringLiteral("未知");
    }
}

QColor orderStatusColor(int s)
{
    switch (s) {
    case OrderCharging: return QColor("#B0863F");
    case OrderFinished: return QColor("#1F9D67");
    case OrderAbnormal: return QColor("#C5525A");
    default:            return QColor("#94A3B8");
    }
}

QString finishText(int f)
{
    switch (f) {
    case FinishByUser:    return QStringLiteral("用户结束");
    case FinishByTarget:  return QStringLiteral("达到目标");
    case FinishByBalance: return QStringLiteral("余额耗尽");
    case FinishByTimeout: return QStringLiteral("超时");
    case FinishByAdmin:   return QStringLiteral("管理员结束");
    case FinishByFault:   return QStringLiteral("故障中断");
    default:              return QString();
    }
}

QString resTypeText(int t) { return t == ReserveAppoint ? QStringLiteral("预约") : QStringLiteral("排队"); }

QString resStatusText(int s)
{
    switch (s) {
    case ReservationActive:    return QStringLiteral("有效");
    case ReservationAssigned:  return QStringLiteral("待确认");
    case ReservationCanceled:  return QStringLiteral("已取消");
    case ReservationExpired:   return QStringLiteral("已过期");
    case ReservationFulfilled: return QStringLiteral("已履约");
    default:                   return QString();
    }
}
} // namespace

OrderManagePage::OrderManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(14);

    QLabel *title = new QLabel(QStringLiteral("订单与预约管理"), this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("manageTabs");

    // ---- Tab1 订单 ----
    QWidget *orderTab = new QWidget(this);
    QVBoxLayout *orderLayout = new QVBoxLayout(orderTab);
    orderLayout->setContentsMargins(0, 12, 0, 0);
    orderLayout->setSpacing(12);

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(QStringLiteral("订单状态:"), orderTab));
    m_statusFilter = new QComboBox(orderTab);
    m_statusFilter->addItem(QStringLiteral("全部"), -1);
    m_statusFilter->addItem(QStringLiteral("充电中"), OrderCharging);
    m_statusFilter->addItem(QStringLiteral("已完成"), OrderFinished);
    m_statusFilter->addItem(QStringLiteral("异常中断"), OrderAbnormal);
    m_statusFilter->addItem(QStringLiteral("已取消"), OrderCancelled);
    m_statusFilter->setFixedWidth(140);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), orderTab);
    refreshBtn->setObjectName("refreshButton");
    m_forceBtn = new QPushButton(QStringLiteral("强制结束"), orderTab);
    m_forceBtn->setObjectName("freezeButton");
    m_refundBtn = new QPushButton(QStringLiteral("退款"), orderTab);
    m_refundBtn->setObjectName("searchButton");
    m_detailBtn = new QPushButton(QStringLiteral("订单详情"), orderTab);
    m_detailBtn->setObjectName("searchButton");
    m_forceBtn->setEnabled(false);
    m_refundBtn->setEnabled(false);
    m_detailBtn->setEnabled(false);

    topRow->addWidget(m_statusFilter);
    topRow->addWidget(refreshBtn);
    topRow->addSpacing(12);
    topRow->addWidget(m_forceBtn);
    topRow->addWidget(m_refundBtn);
    topRow->addWidget(m_detailBtn);
    topRow->addStretch();
    orderLayout->addLayout(topRow);

    m_orderTable = new QTableWidget(orderTab);
    m_orderTable->setObjectName("userTable");
    m_orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_orderTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_orderTable->setAlternatingRowColors(true);
    m_orderTable->verticalHeader()->setVisible(false);
    m_orderTable->setColumnCount(12);
    m_orderTable->setHorizontalHeaderLabels(
        { QStringLiteral("订单号"), QStringLiteral("用户"), QStringLiteral("充电桩"),
          QStringLiteral("充电站"), QStringLiteral("开始时间"), QStringLiteral("结束时间"),
          QStringLiteral("电量(度)"), QStringLiteral("金额(元)"), QStringLiteral("冻结(元)"),
          QStringLiteral("状态"), QStringLiteral("结束原因"), QStringLiteral("已退(元)") });
    orderLayout->addWidget(m_orderTable, 1);
    m_tabs->addTab(orderTab, QStringLiteral("充电订单"));

    // ---- Tab2 排队/预约 ----
    QWidget *resTab = new QWidget(this);
    QVBoxLayout *resLayout = new QVBoxLayout(resTab);
    resLayout->setContentsMargins(0, 12, 0, 0);
    resLayout->setSpacing(12);

    QHBoxLayout *resTop = new QHBoxLayout();
    resTop->addWidget(new QLabel(QStringLiteral("类型:"), resTab));
    m_resFilter = new QComboBox(resTab);
    m_resFilter->addItem(QStringLiteral("全部"), -1);
    m_resFilter->addItem(QStringLiteral("现场排队"), ReserveQueue);
    m_resFilter->addItem(QStringLiteral("时段预约"), ReserveAppoint);
    m_resFilter->setFixedWidth(140);
    QPushButton *resRefreshBtn = new QPushButton(QStringLiteral("刷新"), resTab);
    resRefreshBtn->setObjectName("refreshButton");
    m_cancelResBtn = new QPushButton(QStringLiteral("取消选中"), resTab);
    m_cancelResBtn->setObjectName("freezeButton");
    m_cancelResBtn->setEnabled(false);
    resTop->addWidget(m_resFilter);
    resTop->addWidget(resRefreshBtn);
    resTop->addSpacing(12);
    resTop->addWidget(m_cancelResBtn);
    resTop->addStretch();
    resLayout->addLayout(resTop);

    m_resTable = new QTableWidget(resTab);
    m_resTable->setObjectName("userTable");
    m_resTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resTable->setAlternatingRowColors(true);
    m_resTable->verticalHeader()->setVisible(false);
    m_resTable->setColumnCount(9);
    m_resTable->setHorizontalHeaderLabels(
        { QStringLiteral("ID"), QStringLiteral("类型"), QStringLiteral("用户ID"),
          QStringLiteral("手机号"), QStringLiteral("充电桩"), QStringLiteral("充电站"),
          QStringLiteral("创建时间"), QStringLiteral("时段/排队"), QStringLiteral("状态") });
    resLayout->addWidget(m_resTable, 1);
    m_tabs->addTab(resTab, QStringLiteral("排队 / 预约"));

    layout->addWidget(m_tabs, 1);

    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshOrders(); });
    connect(refreshBtn, &QPushButton::clicked, this, &OrderManagePage::refreshOrders);
    connect(m_forceBtn, &QPushButton::clicked, this, &OrderManagePage::onForceFinish);
    connect(m_refundBtn, &QPushButton::clicked, this, &OrderManagePage::onRefund);
    connect(m_detailBtn, &QPushButton::clicked, this, &OrderManagePage::onShowDetail);
    connect(m_orderTable, &QTableWidget::itemSelectionChanged,
            this, &OrderManagePage::onOrderSelectionChanged);

    connect(m_resFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshReservations(); });
    connect(resRefreshBtn, &QPushButton::clicked, this, &OrderManagePage::refreshReservations);
    connect(m_cancelResBtn, &QPushButton::clicked, this, &OrderManagePage::onCancelReservation);
    connect(m_resTable, &QTableWidget::itemSelectionChanged,
            this, &OrderManagePage::onReservationSelectionChanged);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 0) refreshOrders();
        else refreshReservations();
    });

    refreshOrders();
}

void OrderManagePage::refreshOrders()
{
    const int filter = m_statusFilter->currentData().toInt();
    const QList<OrderInfo> orders = OrderDao::listAll(filter);
    m_orderTable->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const OrderInfo &o = orders[i];
        auto *idItem = new QTableWidgetItem(QString::number(o.id));
        idItem->setData(Qt::UserRole, o.id);
        idItem->setData(Qt::UserRole + 1, o.status);
        idItem->setData(Qt::UserRole + 2, o.amount);
        idItem->setData(Qt::UserRole + 3, o.refundAmount);
        m_orderTable->setItem(i, 0, idItem);
        m_orderTable->setItem(i, 1, new QTableWidgetItem(QString::number(o.userId)));
        m_orderTable->setItem(i, 2, new QTableWidgetItem(o.pileCode));
        m_orderTable->setItem(i, 3, new QTableWidgetItem(o.stationName));
        m_orderTable->setItem(i, 4, new QTableWidgetItem(o.startTime));
        m_orderTable->setItem(i, 5, new QTableWidgetItem(o.endTime));
        m_orderTable->setItem(i, 6, new QTableWidgetItem(QString::number(o.energy, 'f', 2)));
        m_orderTable->setItem(i, 7, new QTableWidgetItem(QString::number(o.amount, 'f', 2)));
        m_orderTable->setItem(i, 8, new QTableWidgetItem(QString::number(o.freezeAmount, 'f', 2)));
        auto *stItem = new QTableWidgetItem(orderStatusText(o.status));
        stItem->setForeground(QBrush(orderStatusColor(o.status)));
        m_orderTable->setItem(i, 9, stItem);
        m_orderTable->setItem(i, 10, new QTableWidgetItem(
            o.status == OrderCharging ? QString() : finishText(o.finishType)));
        m_orderTable->setItem(i, 11, new QTableWidgetItem(
            o.refundAmount > 0 ? QString::number(o.refundAmount, 'f', 2) : QString()));
    }
    m_orderTable->resizeColumnsToContents();
    onOrderSelectionChanged();
}

void OrderManagePage::onOrderSelectionChanged()
{
    const QList<QTableWidgetItem *> sel = m_orderTable->selectedItems();
    if (sel.isEmpty()) {
        m_selectedOrderId = -1;
        m_forceBtn->setEnabled(false);
        m_refundBtn->setEnabled(false);
        m_detailBtn->setEnabled(false);
        return;
    }
    const QTableWidgetItem *idItem = m_orderTable->item(sel.first()->row(), 0);
    m_selectedOrderId = idItem->data(Qt::UserRole).toInt();
    m_selectedOrderStatus = idItem->data(Qt::UserRole + 1).toInt();
    m_selectedOrderAmount = idItem->data(Qt::UserRole + 2).toDouble();
    m_selectedRefunded = idItem->data(Qt::UserRole + 3).toDouble();
    m_forceBtn->setEnabled(m_selectedOrderStatus == OrderCharging);
    m_refundBtn->setEnabled(m_selectedOrderStatus == OrderFinished
                            || m_selectedOrderStatus == OrderAbnormal);
    m_detailBtn->setEnabled(true);
}

void OrderManagePage::onForceFinish()
{
    if (m_selectedOrderId < 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("强制结束"),
                              QStringLiteral("确定强制结束订单 #%1 吗? 将按当前电量结算。")
                                  .arg(m_selectedOrderId)) != QMessageBox::Yes)
        return;
    const ChargingEngine::SettleResult sr =
        ChargingEngine::instance().forceFinish(m_selectedOrderId,
                                               QStringLiteral("管理员强制结束"));
    if (!sr.ok) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), sr.error);
        return;
    }
    LogDao::record(ServerSession::instance().adminName, QStringLiteral("强制结束订单"),
                   QStringLiteral("订单 #%1, 金额 %2 元")
                       .arg(m_selectedOrderId).arg(sr.order.amount, 0, 'f', 2));
    QMessageBox::information(this, QStringLiteral("已结束"),
                             QStringLiteral("订单 #%1 已结算, 消费 %2 元")
                                 .arg(m_selectedOrderId).arg(sr.order.amount, 0, 'f', 2));
    refreshOrders();
}

void OrderManagePage::onRefund()
{
    if (m_selectedOrderId < 0)
        return;
    const double suggest = qMax(0.0, m_selectedOrderAmount - m_selectedRefunded);
    bool ok = false;
    const double amount = QInputDialog::getDouble(
        this, QStringLiteral("订单退款"),
        QStringLiteral("订单 #%1 消费 %2 元, 已退 %3 元, 输入本次退款金额:")
            .arg(m_selectedOrderId)
            .arg(m_selectedOrderAmount, 0, 'f', 2)
            .arg(m_selectedRefunded, 0, 'f', 2),
        suggest, 0.0, 100000.0, 2, &ok);
    if (!ok || amount <= 0)
        return;
    QString err;
    if (!ChargingEngine::refundOrder(m_selectedOrderId, amount, &err)) {
        QMessageBox::warning(this, QStringLiteral("退款失败"), err);
        return;
    }
    LogDao::record(ServerSession::instance().adminName, QStringLiteral("订单退款"),
                   QStringLiteral("订单 #%1 退款 %2 元")
                       .arg(m_selectedOrderId).arg(amount, 0, 'f', 2));
    QMessageBox::information(this, QStringLiteral("退款成功"),
                             QStringLiteral("已向用户退回 %1 元").arg(amount, 0, 'f', 2));
    refreshOrders();
}

void OrderManagePage::onShowDetail()
{
    if (m_selectedOrderId < 0)
        return;
    const OrderInfo o = OrderDao::getById(m_selectedOrderId);
    if (o.id == 0)
        return;
    const QString text = QStringLiteral(
        "订单号: #%1\n用户ID: %2\n充电桩: %3\n充电站: %4\n开始: %5\n结束: %6\n"
        "电量: %7 度\n金额: %8 元\n冻结: %9 元\n计费单价: %10 元/度\n状态: %11\n"
        "结束原因: %12\n目标类型: %13  目标值: %14\n已退款: %15 元\n模拟时长: %16 分钟")
        .arg(o.id).arg(o.userId).arg(o.pileCode, o.stationName, o.startTime, o.endTime)
        .arg(o.energy, 0, 'f', 2).arg(o.amount, 0, 'f', 2)
        .arg(o.freezeAmount, 0, 'f', 2).arg(o.priceSnapshot, 0, 'f', 2)
        .arg(orderStatusText(o.status), finishText(o.finishType))
        .arg(o.targetType).arg(o.targetValue, 0, 'f', 2)
        .arg(o.refundAmount, 0, 'f', 2).arg(o.simMinutes);
    QMessageBox::information(this, QStringLiteral("订单详情 #%1").arg(o.id), text);
}

void OrderManagePage::refreshReservations()
{
    const int typeFilter = m_resFilter->currentData().toInt();
    const QList<ReservationInfo> list = ReservationDao::listAll(-1, typeFilter);
    m_resTable->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const ReservationInfo &r = list[i];
        auto *idItem = new QTableWidgetItem(QString::number(r.id));
        idItem->setData(Qt::UserRole, r.id);
        idItem->setData(Qt::UserRole + 1, r.status);
        m_resTable->setItem(i, 0, idItem);
        m_resTable->setItem(i, 1, new QTableWidgetItem(resTypeText(r.type)));
        m_resTable->setItem(i, 2, new QTableWidgetItem(QString::number(r.userId)));
        m_resTable->setItem(i, 3, new QTableWidgetItem(r.phoneMasked));
        m_resTable->setItem(i, 4, new QTableWidgetItem(r.pileCode));
        m_resTable->setItem(i, 5, new QTableWidgetItem(r.stationName));
        m_resTable->setItem(i, 6, new QTableWidgetItem(r.createTime));
        QString middle;
        if (r.type == ReserveAppoint)
            middle = QStringLiteral("%1 %2~%3").arg(r.reserveDate, r.reserveStart, r.reserveEnd);
        else if (r.status == ReservationAssigned)
            middle = QStringLiteral("轮到, 待确认(至 %1)").arg(r.expireTime);
        else if (r.status == ReservationActive)
            middle = QStringLiteral("排队第 %1 位").arg(qMax(1, r.queuePos));
        m_resTable->setItem(i, 7, new QTableWidgetItem(middle));
        auto *stItem = new QTableWidgetItem(resStatusText(r.status));
        if (r.status == ReservationActive || r.status == ReservationAssigned)
            stItem->setForeground(QBrush(QColor("#B0863F")));
        else if (r.status == ReservationFulfilled)
            stItem->setForeground(QBrush(QColor("#1F9D67")));
        m_resTable->setItem(i, 8, stItem);
    }
    m_resTable->resizeColumnsToContents();
    onReservationSelectionChanged();
}

void OrderManagePage::onReservationSelectionChanged()
{
    const QList<QTableWidgetItem *> sel = m_resTable->selectedItems();
    if (sel.isEmpty()) {
        m_selectedResId = -1;
        m_cancelResBtn->setEnabled(false);
        return;
    }
    const QTableWidgetItem *idItem = m_resTable->item(sel.first()->row(), 0);
    m_selectedResId = idItem->data(Qt::UserRole).toInt();
    m_selectedResStatus = idItem->data(Qt::UserRole + 1).toInt();
    m_cancelResBtn->setEnabled(m_selectedResStatus == ReservationActive
                               || m_selectedResStatus == ReservationAssigned);
}

void OrderManagePage::onCancelReservation()
{
    if (m_selectedResId < 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("取消"),
                              QStringLiteral("确定取消 #%1 的排队/预约吗?").arg(m_selectedResId))
        != QMessageBox::Yes)
        return;
    QString err;
    if (!ReservationDao::cancelByAdmin(m_selectedResId, &err)) {
        QMessageBox::warning(this, QStringLiteral("操作失败"),
                             err.isEmpty() ? QStringLiteral("记录不存在或已结束") : err);
        return;
    }
    LogDao::record(ServerSession::instance().adminName, QStringLiteral("取消排队/预约"),
                   QStringLiteral("记录 #%1").arg(m_selectedResId));
    refreshReservations();
}
