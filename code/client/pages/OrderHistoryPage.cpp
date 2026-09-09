#include "OrderHistoryPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
QString orderStatusText(int status)
{
    switch (status) {
    case OrderCharging:  return QStringLiteral("充电中");
    case OrderFinished:  return QStringLiteral("已完成");
    case OrderWaiting:   return QStringLiteral("等待中");
    case OrderCancelled: return QStringLiteral("已取消");
    case OrderAbnormal:  return QStringLiteral("异常中断");
    default:             return QStringLiteral("状态未知");
    }
}

QString orderStyle(int status)
{
    if (status == OrderCharging) return QStringLiteral("active");
    if (status == OrderFinished) return QStringLiteral("done");
    if (status == OrderAbnormal) return QStringLiteral("error");
    return QStringLiteral("muted");
}

QString reservationTypeText(int)
{
    return QStringLiteral("时段预约");
}

QString reservationStatusText(int status)
{
    switch (status) {
    case ReservationActive:    return QStringLiteral("进行中");
    case ReservationAssigned:  return QStringLiteral("待确认");
    case ReservationCanceled:  return QStringLiteral("已取消");
    case ReservationExpired:   return QStringLiteral("已过期");
    case ReservationFulfilled: return QStringLiteral("已履约");
    default:                   return QStringLiteral("状态未知");
    }
}

QString compactTime(const QString &value, const QString &fallback = QStringLiteral("进行中"))
{
    if (value.trimmed().isEmpty()) return fallback;
    const QDateTime time = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return time.isValid() ? time.toString(QStringLiteral("MM.dd  HH:mm")) : value;
}

void clearCards(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

QFrame *makeMetricPanel(const QString &caption, const QString &value, QWidget *parent,
                        bool featured = false)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName("orderMetric");
    panel->setProperty("featured", featured);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(2);
    auto *captionLabel = new QLabel(caption, panel);
    captionLabel->setObjectName("orderMetricCaption");
    auto *valueLabel = new QLabel(value, panel);
    valueLabel->setObjectName(featured ? "orderAmount" : "orderMetricValue");
    layout->addWidget(captionLabel);
    layout->addWidget(valueLabel);
    return panel;
}

QScrollArea *cardArea(QWidget *parent, QVBoxLayout **cards, const char *hostName)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("orderCardScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget(scroll);
    host->setObjectName(hostName);
    *cards = new QVBoxLayout(host);
    (*cards)->setContentsMargins(0, 0, 7, 0);
    (*cards)->setSpacing(12);
    (*cards)->setAlignment(Qt::AlignTop);
    scroll->setWidget(host);
    return scroll;
}

QLabel *detailField(const QString &caption, const QString &value, QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("%1\n%2").arg(caption, value), parent);
    label->setObjectName("orderDetailField");
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
} // namespace

OrderHistoryPage::OrderHistoryPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("orderHistoryPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 28);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("我的订单"), this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto *hint = new QLabel(QStringLiteral("每一次补能，都留下一张简洁的旅程票据"), this);
    hint->setObjectName("pageHint");
    layout->addWidget(hint);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("orderTabs");

    auto *orderTab = new QWidget(this);
    auto *orderLayout = new QVBoxLayout(orderTab);
    orderLayout->setContentsMargins(0, 14, 0, 0);
    orderLayout->setSpacing(12);
    auto *orderToolbar = new QHBoxLayout;
    m_orderSummary = new QLabel(QStringLiteral("正在整理充电记录…"), orderTab);
    m_orderSummary->setObjectName("orderSummary");
    auto *refreshOrdersButton = new QPushButton(QStringLiteral("刷新记录"), orderTab);
    refreshOrdersButton->setObjectName("secondaryBtn");
    orderToolbar->addWidget(m_orderSummary, 1);
    orderToolbar->addWidget(refreshOrdersButton);
    orderLayout->addLayout(orderToolbar);
    orderLayout->addWidget(cardArea(orderTab, &m_orderCards, "orderCardsHost"), 1);

    auto *pager = new QHBoxLayout;
    m_pageLabel = new QLabel(orderTab);
    m_pageLabel->setObjectName("pageHint");
    m_prevBtn = new QPushButton(QStringLiteral("← 上一页"), orderTab);
    m_prevBtn->setObjectName("orderPagerButton");
    m_nextBtn = new QPushButton(QStringLiteral("下一页 →"), orderTab);
    m_nextBtn->setObjectName("orderPagerButton");
    pager->addWidget(m_pageLabel);
    pager->addStretch();
    pager->addWidget(m_prevBtn);
    pager->addWidget(m_nextBtn);
    orderLayout->addLayout(pager);
    m_tabs->addTab(orderTab, QStringLiteral("充电旅程"));

    auto *reservationTab = new QWidget(this);
    auto *reservationLayout = new QVBoxLayout(reservationTab);
    reservationLayout->setContentsMargins(0, 14, 0, 0);
    reservationLayout->setSpacing(12);
    auto *reservationToolbar = new QHBoxLayout;
    m_reservationSummary = new QLabel(QStringLiteral("正在整理预约记录…"), reservationTab);
    m_reservationSummary->setObjectName("reservationSummary");
    auto *refreshReservationsButton = new QPushButton(QStringLiteral("刷新记录"), reservationTab);
    refreshReservationsButton->setObjectName("secondaryBtn");
    reservationToolbar->addWidget(m_reservationSummary, 1);
    reservationToolbar->addWidget(refreshReservationsButton);
    reservationLayout->addLayout(reservationToolbar);
    reservationLayout->addWidget(cardArea(reservationTab, &m_reservationCards,
                                           "reservationCardsHost"), 1);
    m_tabs->addTab(reservationTab, QStringLiteral("时段预约"));

    layout->addWidget(m_tabs, 1);

    connect(refreshOrdersButton, &QPushButton::clicked, this, [this] {
        m_page = 0;
        refreshOrders();
    });
    connect(m_prevBtn, &QPushButton::clicked, this, &OrderHistoryPage::onPrevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &OrderHistoryPage::onNextPage);
    connect(refreshReservationsButton, &QPushButton::clicked,
            this, &OrderHistoryPage::refreshReservations);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 0) refreshOrders();
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

void OrderHistoryPage::refreshPage()
{
    m_silentRefresh = true;
    if (m_tabs->currentIndex() == 0) refreshOrders();
    else refreshReservations();
    m_silentRefresh = false;
}

QWidget *OrderHistoryPage::createOrderCard(const OrderInfo &order)
{
    auto *card = new QFrame(this);
    card->setObjectName("orderJourneyCard");
    card->setProperty("orderState", orderStyle(order.status));
    card->setProperty("orderId", order.id);
    auto *body = new QVBoxLayout(card);
    body->setContentsMargins(20, 16, 20, 16);
    body->setSpacing(12);

    auto *heading = new QHBoxLayout;
    auto *marker = new QLabel(QStringLiteral("%1").arg(order.id % 100, 2, 10, QChar('0')), card);
    marker->setObjectName("orderMarker");
    auto *nameBox = new QVBoxLayout;
    nameBox->setSpacing(2);
    auto *station = new QLabel(order.stationName.isEmpty() ? QStringLiteral("未命名充电站")
                                                           : order.stationName, card);
    station->setObjectName("orderStation");
    station->setTextFormat(Qt::PlainText);
    auto *identity = new QLabel(QStringLiteral("电桩 %1   ·   NO. %2")
                                    .arg(order.pileCode.isEmpty() ? QStringLiteral("--") : order.pileCode)
                                    .arg(order.id, 6, 10, QChar('0')), card);
    identity->setObjectName("orderIdentity");
    nameBox->addWidget(station);
    nameBox->addWidget(identity);
    auto *status = new QLabel(QStringLiteral("●  %1").arg(orderStatusText(order.status)), card);
    status->setObjectName("orderStateBadge");
    status->setProperty("orderState", orderStyle(order.status));
    heading->addWidget(marker);
    heading->addSpacing(10);
    heading->addLayout(nameBox, 1);
    heading->addWidget(status, 0, Qt::AlignTop);
    body->addLayout(heading);

    auto *timeline = new QLabel(
        QStringLiteral("%1      ─────  充电旅程  ─────→      %2")
            .arg(compactTime(order.startTime, QStringLiteral("时间未知")),
                 compactTime(order.endTime)), card);
    timeline->setObjectName("orderTimeline");
    timeline->setTextFormat(Qt::PlainText);
    timeline->setWordWrap(true);
    body->addWidget(timeline);

    auto *metrics = new QHBoxLayout;
    metrics->setSpacing(8);
    metrics->addWidget(makeMetricPanel(QStringLiteral("本次消费"),
                                       QStringLiteral("¥ %1").arg(order.amount, 0, 'f', 2), card, true), 2);
    metrics->addWidget(makeMetricPanel(QStringLiteral("补能"),
                                       QStringLiteral("%1 度").arg(order.energy, 0, 'f', 2), card), 1);
    metrics->addWidget(makeMetricPanel(QStringLiteral("历时"),
                                       QStringLiteral("%1 分").arg(order.simMinutes), card), 1);
    metrics->addWidget(makeMetricPanel(QStringLiteral("单价"),
                                       QStringLiteral("%1 元/度").arg(order.priceSnapshot, 0, 'f', 2), card), 1);
    body->addLayout(metrics);

    auto *footer = new QHBoxLayout;
    if (order.refundAmount > 0) {
        auto *refund = new QLabel(QStringLiteral("已退款 ¥ %1").arg(order.refundAmount, 0, 'f', 2), card);
        refund->setObjectName("orderRefund");
        footer->addWidget(refund);
    } else {
        auto *caption = new QLabel(order.status == OrderCharging
                                       ? QStringLiteral("费用随充电进度实时更新")
                                       : QStringLiteral("已按实际充电量完成计费"), card);
        caption->setObjectName("orderFootnote");
        footer->addWidget(caption);
    }
    footer->addStretch();
    auto *detail = new QPushButton(QStringLiteral("查看票据  ↗"), card);
    detail->setObjectName("orderDetailButton");
    detail->setProperty("orderId", order.id);
    footer->addWidget(detail);
    body->addLayout(footer);
    connect(detail, &QPushButton::clicked, this, [this, id = order.id] {
        m_selectedOrderId = id;
        onShowDetail();
    });
    return card;
}

QWidget *OrderHistoryPage::createReservationCard(const ReservationInfo &reservation)
{
    auto *card = new QFrame(this);
    card->setObjectName("reservationTicket");
    card->setProperty("reservationActive", reservation.status == ReservationActive
                                               || reservation.status == ReservationAssigned);
    auto *body = new QVBoxLayout(card);
    body->setContentsMargins(20, 16, 20, 16);
    body->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *type = new QLabel(reservationTypeText(reservation.type), card);
    type->setObjectName("reservationType");
    auto *station = new QLabel(reservation.stationName.isEmpty() ? QStringLiteral("未命名充电站")
                                                                  : reservation.stationName, card);
    station->setObjectName("reservationStation");
    station->setTextFormat(Qt::PlainText);
    auto *status = new QLabel(reservationStatusText(reservation.status), card);
    status->setObjectName("reservationStatus");
    status->setProperty("reservationState",
                        reservation.status == ReservationActive || reservation.status == ReservationAssigned
                            ? "active" : reservation.status == ReservationFulfilled ? "done" : "muted");
    top->addWidget(type);
    top->addSpacing(10);
    top->addWidget(station, 1);
    top->addWidget(status);
    body->addLayout(top);

    const QString schedule = QStringLiteral("%1   %2—%3")
                                 .arg(reservation.reserveDate, reservation.reserveStart,
                                      reservation.reserveEnd);
    auto *journey = new QLabel(QStringLiteral("电桩 %1      ·      %2")
                                   .arg(reservation.pileCode.isEmpty() ? QStringLiteral("--")
                                                                       : reservation.pileCode,
                                        schedule), card);
    journey->setObjectName("reservationJourney");
    journey->setTextFormat(Qt::PlainText);
    journey->setWordWrap(true);
    body->addWidget(journey);

    auto *footer = new QHBoxLayout;
    auto *created = new QLabel(QStringLiteral("创建于 %1   ·   #%2")
                                   .arg(compactTime(reservation.createTime, QStringLiteral("时间未知")))
                                   .arg(reservation.id), card);
    created->setObjectName("orderFootnote");
    footer->addWidget(created);
    footer->addStretch();
    if (reservation.status == ReservationActive || reservation.status == ReservationAssigned) {
        auto *cancel = new QPushButton(QStringLiteral("取消预约"), card);
        cancel->setObjectName("reservationCancelButton");
        footer->addWidget(cancel);
        connect(cancel, &QPushButton::clicked, this,
                [this, id = reservation.id, status = reservation.status] {
                    m_selectedResId = id;
                    m_selectedResStatus = status;
                    onCancelReservation();
                });
    }
    body->addLayout(footer);
    return card;
}

void OrderHistoryPage::refreshOrders()
{
    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    request.insert("page", m_page);
    request.insert("pageSize", m_pageSize);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqOrderHistory, request);
    if (!reply.value("ok").toBool()) {
        if (!m_silentRefresh)
            QMessageBox::warning(this, QStringLiteral("加载失败"), reply.value("error").toString());
        return;
    }

    m_total = reply.value("total").toInt();
    const QJsonArray orders = reply.value("orders").toArray();
    clearCards(m_orderCards);
    double totalEnergy = 0.0;
    double totalAmount = 0.0;
    for (const QJsonValue &value : orders) {
        const OrderInfo order = OrderInfo::fromJson(value.toObject());
        totalEnergy += order.energy;
        totalAmount += order.amount;
        m_orderCards->addWidget(createOrderCard(order));
    }
    if (orders.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("还没有充电旅程\n\n完成第一次充电后，这里会生成一张专属票据"), this);
        empty->setObjectName("orderEmpty");
        empty->setAlignment(Qt::AlignCenter);
        m_orderCards->addWidget(empty);
    }
    m_orderSummary->setText(QStringLiteral("本页 %1 笔旅程   ·   %2 度   ·   ¥ %3")
                                .arg(orders.size()).arg(totalEnergy, 0, 'f', 2)
                                .arg(totalAmount, 0, 'f', 2));

    const int totalPages = qMax(1, (m_total + m_pageSize - 1) / m_pageSize);
    m_pageLabel->setText(QStringLiteral("%1 / %2 页   ·   共 %3 单")
                             .arg(m_page + 1).arg(totalPages).arg(m_total));
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
    if (m_selectedOrderId < 0) return;
    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    request.insert("orderId", m_selectedOrderId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqOrderDetail, request);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), reply.value("error").toString());
        return;
    }
    const OrderInfo order = OrderInfo::fromJson(reply.value("order").toObject());

    QDialog dialog(this);
    dialog.setObjectName("orderReceiptDialog");
    dialog.setWindowTitle(QStringLiteral("充电票据 #%1").arg(order.id));
    dialog.resize(560, 500);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);

    auto *hero = new QFrame(&dialog);
    hero->setObjectName("orderReceiptHero");
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(22, 18, 22, 18);
    auto *heroCaption = new QLabel(QStringLiteral("CHARGE RECEIPT  ·  NO. %1")
                                       .arg(order.id, 6, 10, QChar('0')), hero);
    heroCaption->setObjectName("orderReceiptCaption");
    auto *heroAmount = new QLabel(QStringLiteral("¥ %1").arg(order.amount, 0, 'f', 2), hero);
    heroAmount->setObjectName("orderReceiptAmount");
    auto *heroStation = new QLabel(QStringLiteral("%1  ·  %2")
                                       .arg(order.stationName, orderStatusText(order.status)), hero);
    heroStation->setObjectName("orderReceiptStation");
    heroStation->setTextFormat(Qt::PlainText);
    heroLayout->addWidget(heroCaption);
    heroLayout->addWidget(heroAmount);
    heroLayout->addWidget(heroStation);
    layout->addWidget(hero);

    auto *fields = new QGridLayout;
    fields->setSpacing(10);
    fields->addWidget(detailField(QStringLiteral("电桩"), order.pileCode, &dialog), 0, 0);
    fields->addWidget(detailField(QStringLiteral("充电电量"),
                                  QStringLiteral("%1 度").arg(order.energy, 0, 'f', 2), &dialog), 0, 1);
    fields->addWidget(detailField(QStringLiteral("充电时长"),
                                  QStringLiteral("%1 分钟").arg(order.simMinutes), &dialog), 1, 0);
    fields->addWidget(detailField(QStringLiteral("计费单价"),
                                  QStringLiteral("%1 元/度").arg(order.priceSnapshot, 0, 'f', 2), &dialog), 1, 1);
    fields->addWidget(detailField(QStringLiteral("开始时间"),
                                  order.startTime.isEmpty() ? QStringLiteral("--") : order.startTime, &dialog), 2, 0);
    fields->addWidget(detailField(QStringLiteral("结束时间"),
                                  order.endTime.isEmpty() ? QStringLiteral("进行中") : order.endTime, &dialog), 2, 1);
    fields->addWidget(detailField(QStringLiteral("退款金额"),
                                  QStringLiteral("¥ %1").arg(order.refundAmount, 0, 'f', 2), &dialog), 3, 0, 1, 2);
    layout->addLayout(fields);
    layout->addStretch();
    auto *close = new QPushButton(QStringLiteral("收好票据"), &dialog);
    close->setObjectName("primaryBtn");
    close->setMinimumWidth(130);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    actions->addWidget(close);
    layout->addLayout(actions);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void OrderHistoryPage::refreshReservations()
{
    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqMyReservations, request);
    if (!reply.value("ok").toBool()) return;

    const QJsonArray allReservations = reply.value("reservations").toArray();
    clearCards(m_reservationCards);
    int active = 0;
    int count = 0;
    for (const QJsonValue &value : allReservations) {
        const ReservationInfo reservation = ReservationInfo::fromJson(value.toObject());
        if (reservation.type != ReserveAppoint)
            continue;
        ++count;
        if (reservation.status == ReservationActive || reservation.status == ReservationAssigned)
            ++active;
        m_reservationCards->addWidget(createReservationCard(reservation));
    }
    if (count == 0) {
        auto *empty = new QLabel(QStringLiteral("当前没有时段预约\n\n选择任意正常电桩，即可预约充电时段"), this);
        empty->setObjectName("orderEmpty");
        empty->setAlignment(Qt::AlignCenter);
        m_reservationCards->addWidget(empty);
    }
    m_reservationSummary->setText(QStringLiteral("%1 条记录   ·   %2 条正在进行")
                                      .arg(count).arg(active));
}

void OrderHistoryPage::onCancelReservation()
{
    if (m_selectedResId < 0
        || (m_selectedResStatus != ReservationActive
            && m_selectedResStatus != ReservationAssigned)) return;
    if (QMessageBox::question(this, QStringLiteral("取消预约"),
                              QStringLiteral("确定取消 #%1 预约吗？").arg(m_selectedResId))
        != QMessageBox::Yes) return;

    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    request.insert("action", 1);
    request.insert("reservationId", m_selectedResId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqReservePile, request);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), reply.value("error").toString());
        return;
    }
    m_selectedResId = -1;
    m_selectedResStatus = -1;
    refreshReservations();
}
