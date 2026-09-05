#include "OrderHistoryPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "types.h"
#include "network/TcpClient.h"

#include <QBrush>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
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
    default:             return QString();
    }
}

QString resTypeText(int t) { return t == ReserveAppoint ? QStringLiteral("时段预约") : QStringLiteral("现场排队"); }

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

OrderHistoryPage::OrderHistoryPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 24);
    lay->setSpacing(14);

    QLabel *title = new QLabel(QStringLiteral("我的订单"), this);
    title->setObjectName("pageTitle");
    lay->addWidget(title);

    m_tabs = new QTabWidget(this);

    // ---- 订单历史 ----
    QWidget *orderTab = new QWidget(this);
    QVBoxLayout *oLay = new QVBoxLayout(orderTab);
    oLay->setContentsMargins(0, 12, 0, 0);
    oLay->setSpacing(10);

    QHBoxLayout *oTop = new QHBoxLayout();
    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), orderTab);
    refreshBtn->setObjectName("searchButton");
    m_detailBtn = new QPushButton(QStringLiteral("订单详情"), orderTab);
    m_detailBtn->setObjectName("searchButton");
    m_detailBtn->setEnabled(false);
    m_prevBtn = new QPushButton(QStringLiteral("上一页"), orderTab);
    m_nextBtn = new QPushButton(QStringLiteral("下一页"), orderTab);
    m_pageLabel = new QLabel(orderTab);
    m_pageLabel->setStyleSheet("color:#6B7280;");
    oTop->addWidget(refreshBtn);
    oTop->addWidget(m_detailBtn);
    oTop->addStretch();
    oTop->addWidget(m_prevBtn);
    oTop->addWidget(m_pageLabel);
    oTop->addWidget(m_nextBtn);
    oLay->addLayout(oTop);

    m_orderTable = new QTableWidget(orderTab);
    m_orderTable->setObjectName("pileTable");
    m_orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_orderTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_orderTable->setAlternatingRowColors(true);
    m_orderTable->verticalHeader()->setVisible(false);
    m_orderTable->setColumnCount(9);
    m_orderTable->setHorizontalHeaderLabels(
        { QStringLiteral("订单号"), QStringLiteral("电桩"), QStringLiteral("充电站"),
          QStringLiteral("开始时间"), QStringLiteral("结束时间"), QStringLiteral("电量(度)"),
          QStringLiteral("金额(元)"), QStringLiteral("时长(分)"), QStringLiteral("状态") });
    oLay->addWidget(m_orderTable, 1);
    m_tabs->addTab(orderTab, QStringLiteral("充电订单"));

    // ---- 我的预约 ----
    QWidget *resTab = new QWidget(this);
    QVBoxLayout *rLay = new QVBoxLayout(resTab);
    rLay->setContentsMargins(0, 12, 0, 0);
    rLay->setSpacing(10);

    QHBoxLayout *rTop = new QHBoxLayout();
    QPushButton *rRefresh = new QPushButton(QStringLiteral("刷新"), resTab);
    rRefresh->setObjectName("searchButton");
    m_cancelResBtn = new QPushButton(QStringLiteral("取消选中"), resTab);
    m_cancelResBtn->setObjectName("freezeButton");
    m_cancelResBtn->setEnabled(false);
    rTop->addWidget(rRefresh);
    rTop->addWidget(m_cancelResBtn);
    rTop->addStretch();
    rLay->addLayout(rTop);

    m_resTable = new QTableWidget(resTab);
    m_resTable->setObjectName("pileTable");
    m_resTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resTable->setAlternatingRowColors(true);
    m_resTable->verticalHeader()->setVisible(false);
    m_resTable->setColumnCount(8);
    m_resTable->setHorizontalHeaderLabels(
        { QStringLiteral("ID"), QStringLiteral("类型"), QStringLiteral("电桩"),
          QStringLiteral("充电站"), QStringLiteral("创建时间"), QStringLiteral("预约时段"),
          QStringLiteral("排队位"), QStringLiteral("状态") });
    rLay->addWidget(m_resTable, 1);
    m_tabs->addTab(resTab, QStringLiteral("排队 / 预约"));

    lay->addWidget(m_tabs, 1);

    connect(refreshBtn, &QPushButton::clicked, this, [this] { m_page = 0; refreshOrders(); });
    connect(m_prevBtn, &QPushButton::clicked, this, &OrderHistoryPage::onPrevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &OrderHistoryPage::onNextPage);
    connect(m_detailBtn, &QPushButton::clicked, this, &OrderHistoryPage::onShowDetail);
    connect(m_orderTable, &QTableWidget::itemSelectionChanged, this, [this] {
        m_detailBtn->setEnabled(!m_orderTable->selectedItems().isEmpty());
    });
    connect(m_orderTable, &QTableWidget::cellDoubleClicked,
            this, &OrderHistoryPage::onShowDetail);

    connect(rRefresh, &QPushButton::clicked, this, &OrderHistoryPage::refreshReservations);
    connect(m_cancelResBtn, &QPushButton::clicked, this, &OrderHistoryPage::onCancelReservation);
    connect(m_resTable, &QTableWidget::itemSelectionChanged, this, [this] {
        const auto sel = m_resTable->selectedItems();
        if (sel.isEmpty()) {
            m_cancelResBtn->setEnabled(false);
            return;
        }
        const QTableWidgetItem *it = m_resTable->item(sel.first()->row(), 0);
        m_selectedResId = it->data(Qt::UserRole).toInt();
        m_selectedResStatus = it->data(Qt::UserRole + 1).toInt();
        m_cancelResBtn->setEnabled(m_selectedResStatus == ReservationActive
                                   || m_selectedResStatus == ReservationAssigned);
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 0) refreshOrders();
        else refreshReservations();
    });
}

void OrderHistoryPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_page = 0;
    refreshOrders();
    refreshReservations();
}

void OrderHistoryPage::refreshOrders()
{
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("page", m_page);
    req.insert("pageSize", m_pageSize);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqOrderHistory, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), reply.value("error").toString());
        return;
    }
    m_total = reply.value("total").toInt();
    const QJsonArray arr = reply.value("orders").toArray();
    m_orderTable->setRowCount(arr.size());
    for (int i = 0; i < arr.size(); ++i) {
        const OrderInfo o = OrderInfo::fromJson(arr[i].toObject());
        auto *idItem = new QTableWidgetItem(QString::number(o.id));
        idItem->setData(Qt::UserRole, o.id);
        m_orderTable->setItem(i, 0, idItem);
        m_orderTable->setItem(i, 1, new QTableWidgetItem(o.pileCode));
        m_orderTable->setItem(i, 2, new QTableWidgetItem(o.stationName));
        m_orderTable->setItem(i, 3, new QTableWidgetItem(o.startTime));
        m_orderTable->setItem(i, 4, new QTableWidgetItem(o.endTime));
        m_orderTable->setItem(i, 5, new QTableWidgetItem(QString::number(o.energy, 'f', 2)));
        m_orderTable->setItem(i, 6, new QTableWidgetItem(QString::number(o.amount, 'f', 2)));
        m_orderTable->setItem(i, 7, new QTableWidgetItem(QString::number(o.simMinutes)));
        auto *st = new QTableWidgetItem(orderStatusText(o.status));
        st->setForeground(QBrush(o.status == OrderFinished ? QColor("#1F9D67")
                                : o.status == OrderCharging ? QColor("#B0863F")
                                : o.status == OrderAbnormal ? QColor("#C5525A") : QColor("#94A3B8")));
        m_orderTable->setItem(i, 8, st);
    }
    m_orderTable->resizeColumnsToContents();

    const int totalPages = qMax(1, (m_total + m_pageSize - 1) / m_pageSize);
    m_pageLabel->setText(QStringLiteral("第 %1/%2 页 · 共 %3 单").arg(m_page + 1).arg(totalPages).arg(m_total));
    m_prevBtn->setEnabled(m_page > 0);
    m_nextBtn->setEnabled(m_page + 1 < totalPages);
}

void OrderHistoryPage::onPrevPage()
{
    if (m_page > 0) {
        --m_page;
        refreshOrders();
    }
}

void OrderHistoryPage::onNextPage()
{
    const int totalPages = qMax(1, (m_total + m_pageSize - 1) / m_pageSize);
    if (m_page + 1 < totalPages) {
        ++m_page;
        refreshOrders();
    }
}

void OrderHistoryPage::onShowDetail()
{
    const auto sel = m_orderTable->selectedItems();
    if (sel.isEmpty())
        return;
    const int orderId = m_orderTable->item(sel.first()->row(), 0)->data(Qt::UserRole).toInt();
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("orderId", orderId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqOrderDetail, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), reply.value("error").toString());
        return;
    }
    const OrderInfo o = OrderInfo::fromJson(reply.value("order").toObject());
    QMessageBox::information(this, QStringLiteral("订单详情 #%1").arg(o.id),
        QStringLiteral("订单号: #%1\n电桩: %2\n充电站: %3\n开始: %4\n结束: %5\n"
                       "电量: %6 度\n金额: %7 元\n单价: %8 元/度\n冻结: %9 元\n"
                       "时长: %10 分钟\n状态: %11\n退款: %12 元")
            .arg(o.id).arg(o.pileCode, o.stationName, o.startTime, o.endTime)
            .arg(o.energy, 0, 'f', 2).arg(o.amount, 0, 'f', 2)
            .arg(o.priceSnapshot, 0, 'f', 2).arg(o.freezeAmount, 0, 'f', 2)
            .arg(o.simMinutes).arg(orderStatusText(o.status))
            .arg(o.refundAmount, 0, 'f', 2));
}

void OrderHistoryPage::refreshReservations()
{
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqMyReservations, req);
    if (!reply.value("ok").toBool())
        return;
    const QJsonArray arr = reply.value("reservations").toArray();
    m_resTable->setRowCount(arr.size());
    for (int i = 0; i < arr.size(); ++i) {
        const ReservationInfo r = ReservationInfo::fromJson(arr[i].toObject());
        auto *idItem = new QTableWidgetItem(QString::number(r.id));
        idItem->setData(Qt::UserRole, r.id);
        idItem->setData(Qt::UserRole + 1, r.status);
        m_resTable->setItem(i, 0, idItem);
        m_resTable->setItem(i, 1, new QTableWidgetItem(resTypeText(r.type)));
        m_resTable->setItem(i, 2, new QTableWidgetItem(r.pileCode));
        m_resTable->setItem(i, 3, new QTableWidgetItem(r.stationName));
        m_resTable->setItem(i, 4, new QTableWidgetItem(r.createTime));
        QString middle;
        if (r.type == ReserveAppoint)
            middle = QStringLiteral("%1 %2~%3").arg(r.reserveDate, r.reserveStart, r.reserveEnd);
        else if (r.status == ReservationAssigned)
            middle = QStringLiteral("轮到, 待确认");
        else
            middle = QStringLiteral("第 %1 位").arg(qMax(1, r.queuePos));
        m_resTable->setItem(i, 5, new QTableWidgetItem(middle));
        m_resTable->setItem(i, 6, new QTableWidgetItem(QString::number(qMax(0, r.queuePos))));
        auto *st = new QTableWidgetItem(resStatusText(r.status));
        st->setForeground(QBrush(r.status == ReservationActive || r.status == ReservationAssigned
                                 ? QColor("#B0863F") : QColor("#94A3B8")));
        m_resTable->setItem(i, 7, st);
    }
    m_resTable->resizeColumnsToContents();
}

void OrderHistoryPage::onCancelReservation()
{
    if (m_selectedResId < 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("提示"),
                              QStringLiteral("确定取消 #%1 的排队/预约吗?").arg(m_selectedResId))
        != QMessageBox::Yes)
        return;
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("action", 1);
    req.insert("reservationId", m_selectedResId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqReservePile, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), reply.value("error").toString());
        return;
    }
    refreshReservations();
}
