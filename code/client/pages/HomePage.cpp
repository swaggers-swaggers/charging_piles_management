#include "HomePage.h"

#include "ClientSession.h"
#include "IconFactory.h"
#include "MessageCenter.h"
#include "network/TcpClient.h"
#include "protocol.h"
#include "types.h"

#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {
void passMouseToCard(QWidget *widget)
{
    widget->setAttribute(Qt::WA_TransparentForMouseEvents);
}

QColor iconColorForTone(const QString &tone)
{
    if (tone == "hero") return QColor("#E9F5F8");
    if (tone == "green") return QColor("#4F876A");
    if (tone == "amber") return QColor("#9A7138");
    return QColor("#4C7895");
}

QVBoxLayout *cardBody(QPushButton *card)
{
    return qobject_cast<QVBoxLayout *>(card->layout());
}

QLabel *detailLabel(const QString &text, QWidget *parent, const char *name = "homeDetailLine")
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(name);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    passMouseToCard(label);
    return label;
}

QFrame *statBlock(const QString &caption, QLabel **value, QWidget *parent)
{
    auto *block = new QFrame(parent);
    block->setObjectName("homeStatBlock");
    passMouseToCard(block);
    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(2);
    *value = detailLabel("--", block, "homeStatValue");
    auto *captionLabel = detailLabel(caption, block, "homeStatCaption");
    layout->addWidget(*value);
    layout->addWidget(captionLabel);
    return block;
}

QString orderStatusText(int status)
{
    switch (status) {
    case OrderCharging: return QStringLiteral("充电中");
    case OrderFinished: return QStringLiteral("已完成");
    case OrderWaiting: return QStringLiteral("排队中");
    case OrderCancelled: return QStringLiteral("已取消");
    case OrderAbnormal: return QStringLiteral("异常中断");
    default: return QStringLiteral("状态未知");
    }
}
}

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("homePage");
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(28, 24, 28, 30);
    pageLayout->setSpacing(18);

    auto *welcomeRow = new QHBoxLayout;
    auto *welcomeCopy = new QVBoxLayout;
    welcomeCopy->setSpacing(4);
    m_greeting = new QLabel(this);
    m_greeting->setObjectName("homeGreeting");
    m_pageSummary = new QLabel("正在整理今天的补能概览…", this);
    m_pageSummary->setObjectName("homeIntro");
    m_pageSummary->setWordWrap(true);
    welcomeCopy->addWidget(m_greeting);
    welcomeCopy->addWidget(m_pageSummary);

    auto *refresh = new QPushButton("刷新概览", this);
    refresh->setObjectName("homeRefreshButton");
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QPushButton::clicked, this, [this] {
        m_loaded = false;
        loadNetworkDetails();
    });
    welcomeRow->addLayout(welcomeCopy, 1);
    welcomeRow->addWidget(refresh, 0, Qt::AlignVCenter);
    pageLayout->addLayout(welcomeRow);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(16);
    m_grid->setVerticalSpacing(16);

    auto *hero = createBaseCard("开启今天的绿色旅程",
                                "账户、站点与消息，一眼掌握",
                                IconFactory::IconPlug, "hero", "wide", 1);
    auto *heroBody = cardBody(hero);
    auto *heroStats = new QHBoxLayout;
    heroStats->setSpacing(10);
    heroStats->addWidget(statBlock("账户余额", &m_heroBalance, hero));
    heroStats->addWidget(statBlock("附近空闲桩", &m_heroIdle, hero));
    heroStats->addWidget(statBlock("未读消息", &m_heroUnread, hero));
    heroBody->addLayout(heroStats);
    addAction(hero, "查看附近充电站  →");

    auto *connection = createBaseCard("服务连接", "当前客户端连接地址",
                                      IconFactory::IconCompass, "paper", "regular", 7,
                                      &m_connectionStatus);
    auto *connectionBody = cardBody(connection);
    m_endpoint = detailLabel("--:--", connection, "homeEndpoint");
    connectionBody->addWidget(m_endpoint);
    connectionBody->addWidget(detailLabel("登录页可切换 IP 与端口，成功后自动保存", connection));
    addAction(connection, "查看账户与连接信息  →");

    auto *charge = createBaseCard("充电服务", "当前任务与实时结算摘要",
                                  IconFactory::IconBolt, "green", "wide", 2,
                                  &m_chargeStatus);
    auto *chargeBody = cardBody(charge);
    m_chargeTitle = detailLabel("正在查询进行中的充电任务…", charge, "homeFeatureTitle");
    chargeBody->addWidget(m_chargeTitle);
    auto *chargeStats = new QHBoxLayout;
    chargeStats->setSpacing(10);
    chargeStats->addWidget(statBlock("已充电量", &m_chargeEnergy, charge));
    chargeStats->addWidget(statBlock("当前费用", &m_chargeAmount, charge));
    chargeStats->addWidget(statBlock("已用时长", &m_chargeMinutes, charge));
    chargeBody->addLayout(chargeStats);
    addAction(charge, "进入充电服务  →");

    auto *account = createBaseCard("我的账户", "个人资料与可用余额",
                                   IconFactory::IconUser, "amber", "regular", 7);
    auto *accountBody = cardBody(account);
    m_accountBalance = detailLabel("¥ --", account, "homeBalance");
    m_accountName = detailLabel("--", account, "homeFeatureTitle");
    m_accountPhone = detailLabel("--", account);
    accountBody->addWidget(m_accountBalance);
    accountBody->addWidget(m_accountName);
    accountBody->addWidget(m_accountPhone);
    accountBody->addStretch();
    addAction(account, "充值或修改资料  →");

    auto *stations = createBaseCard("附近充电站", "距离、空闲与价格",
                                    IconFactory::IconLocation, "paper", "detail", 1,
                                    &m_stationSummary);
    auto *stationBody = cardBody(stations);
    for (int i = 0; i < 3; ++i) {
        auto *line = detailLabel(i == 0 ? "正在查找站点…" : "", stations);
        line->setProperty("detailRow", true);
        m_stationLines.append(line);
        stationBody->addWidget(line);
    }
    stationBody->addStretch();
    addAction(stations, "查看全部站点  →");

    auto *orders = createBaseCard("预约与订单", "最近两笔充电旅程",
                                  IconFactory::IconChartLine, "paper", "detail", 3,
                                  &m_orderSummary);
    auto *orderBody = cardBody(orders);
    for (int i = 0; i < 2; ++i) {
        auto *line = detailLabel(i == 0 ? "正在整理订单…" : "", orders);
        line->setProperty("detailRow", true);
        m_orderLines.append(line);
        orderBody->addWidget(line);
    }
    orderBody->addStretch();
    addAction(orders, "查看订单与预约  →");

    auto *messages = createBaseCard("消息通知", "充电动态与服务提醒",
                                    IconFactory::IconBattery, "paper", "detail", 6,
                                    &m_messageSummary);
    auto *messageBody = cardBody(messages);
    for (int i = 0; i < 2; ++i) {
        auto *line = detailLabel("", messages);
        line->setProperty("detailRow", true);
        m_messageLines.append(line);
        messageBody->addWidget(line);
    }
    messageBody->addStretch();
    addAction(messages, "查看全部消息  →");
    m_cards = {hero, connection, charge, account, stations, orders, messages};
    relayoutCards(width());

    pageLayout->addLayout(m_grid, 1);
    auto *footnote = new QLabel("查找充电站  ·  选择电桩  ·  导航或开始充电", this);
    footnote->setObjectName("homeFootnote");
    footnote->setAlignment(Qt::AlignCenter);
    pageLayout->addWidget(footnote);

    connect(&MessageCenter::instance(), &MessageCenter::unreadCountChanged,
            this, [this](int) { refreshLocalDetails(); });
    connect(&MessageCenter::instance(), &MessageCenter::messageReceived,
            this, [this](const AppMessage &) { refreshLocalDetails(); });
    connect(&TcpClient::instance(), &TcpClient::connectionLost,
            this, &HomePage::refreshLocalDetails);
    connect(&TcpClient::instance(), &TcpClient::pushReceived, this,
            [this](const QJsonObject &message) {
        if (message.value("type").toInt() != Protocol::PushOrderProgress) return;
        m_chargeStatus->setText("充电中");
        m_chargeEnergy->setText(QString("%1 kWh").arg(message.value("energy").toDouble(), 0, 'f', 2));
        m_chargeAmount->setText(QString("¥ %1").arg(message.value("amount").toDouble(), 0, 'f', 2));
        m_chargeMinutes->setText(QString("%1 分钟").arg(message.value("minutes").toInt()));
    });
    refreshLocalDetails();
}

QPushButton *HomePage::createBaseCard(const QString &title, const QString &subtitle,
                                      int iconType, const QString &tone,
                                      const QString &size, int pageIndex,
                                      QLabel **badge)
{
    auto *card = new QPushButton(this);
    card->setObjectName("bentoCard");
    card->setProperty("bentoTone", tone);
    card->setProperty("bentoSize", size);
    card->setProperty("microScale", size == "wide" ? 1.035 : 1.04);
    card->setCursor(Qt::PointingHandCursor);
    card->setText(QString());
    card->setAccessibleName(title);
    card->setAccessibleDescription(subtitle);
    card->setToolTip(QString("%1：%2").arg(title, subtitle));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *body = new QVBoxLayout(card);
    body->setContentsMargins(size == "wide" ? 22 : 18, 18,
                             size == "wide" ? 22 : 18, 17);
    body->setSpacing(8);
    auto *top = new QHBoxLayout;
    top->setSpacing(10);
    auto *icon = new QLabel(card);
    icon->setObjectName("bentoIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(IconFactory::icon(static_cast<IconFactory::IconType>(iconType),
                                      iconColorForTone(tone), 28).pixmap(28, 28));
    icon->setFixedSize(42, 42);
    auto *heading = new QVBoxLayout;
    heading->setSpacing(2);
    auto *titleLabel = detailLabel(title, card, "bentoTitle");
    auto *subtitleLabel = detailLabel(subtitle, card, "bentoDescription");
    heading->addWidget(titleLabel);
    heading->addWidget(subtitleLabel);
    top->addWidget(icon);
    top->addLayout(heading, 1);
    if (badge) {
        *badge = detailLabel("--", card, "homeBadge");
        (*badge)->setAlignment(Qt::AlignCenter);
        top->addWidget(*badge, 0, Qt::AlignTop);
    }
    body->addLayout(top);
    connect(card, &QPushButton::clicked, this, [this, pageIndex] {
        emit pageRequested(pageIndex);
    });
    return card;
}

void HomePage::addAction(QPushButton *card, const QString &text)
{
    cardBody(card)->addWidget(detailLabel(text, card, "bentoAction"));
}

void HomePage::refreshLocalDetails()
{
    const int hour = QTime::currentTime().hour();
    const QString salutation = hour < 6 ? "夜深了" : hour < 11 ? "早上好" :
                               hour < 14 ? "中午好" : hour < 18 ? "下午好" : "晚上好";
    const auto &session = ClientSession::instance();
    const QString nickname = session.nickname.trimmed().isEmpty()
        ? QStringLiteral("车主") : session.nickname;
    m_greeting->setText(QString("%1，%2").arg(salutation, nickname));

    const int unread = MessageCenter::instance().unreadCount();
    m_heroBalance->setText(QString("¥ %1").arg(session.balance, 0, 'f', 2));
    m_heroUnread->setText(QString::number(unread));
    m_accountBalance->setText(QString("¥ %1").arg(session.balance, 0, 'f', 2));
    m_accountName->setText(nickname);
    m_accountPhone->setText(QString("账号 %1").arg(session.phone.isEmpty() ? "--" : session.phone));

    m_endpoint->setText(QString("%1:%2")
        .arg(TcpClient::instance().serverHost())
        .arg(TcpClient::instance().serverPort()));
    m_connectionStatus->setText(TcpClient::instance().isConnected() ? "已连接" : "连接待确认");

    const QList<AppMessage> messages = MessageCenter::instance().messages();
    m_messageSummary->setText(unread > 0 ? QString("%1 未读").arg(unread) : "全部已读");
    for (int i = 0; i < m_messageLines.size(); ++i) {
        if (i < messages.size()) {
            const AppMessage &message = messages[i];
            m_messageLines[i]->setText(QString("%1\n%2 · %3")
                .arg(message.title,
                     message.time.isValid() ? message.time.toString("MM-dd HH:mm") : QStringLiteral("刚刚"),
                     message.content));
        } else {
            m_messageLines[i]->setText(i == 0 ? "暂无新消息\n充电与预约动态将在这里出现" : "");
        }
    }
}

void HomePage::loadNetworkDetails()
{
    if (m_loading || !isVisible()) return;
    m_loading = true;
    refreshLocalDetails();
    m_pageSummary->setText("正在更新站点、充电与订单信息…");

    const QJsonObject stationReply = TcpClient::instance().request(
        Protocol::ReqStationList, QJsonObject{{"lon", 116.3100}, {"lat", 39.9600}}, 1600);
    if (stationReply.value("ok").toBool()) {
        const QJsonArray stations = stationReply.value("stations").toArray();
        int idle = 0;
        for (const QJsonValue &value : stations)
            idle += StationInfo::fromJson(value.toObject()).idlePiles;
        m_heroIdle->setText(QString::number(idle));
        m_stationSummary->setText(QString("%1 个站点").arg(stations.size()));
        for (int i = 0; i < m_stationLines.size(); ++i) {
            if (i < stations.size()) {
                const StationInfo station = StationInfo::fromJson(stations[i].toObject());
                m_stationLines[i]->setText(QString("%1\n%2 km · 空闲 %3/%4 · ¥%5/度")
                    .arg(station.name)
                    .arg(station.distance >= 0 ? QString::number(station.distance, 'f', 1) : "--")
                    .arg(station.idlePiles).arg(station.totalPiles)
                    .arg(station.price, 0, 'f', 2));
            } else {
                m_stationLines[i]->setText(i == 0 ? "暂无站点数据\n可进入站点页重试" : "");
            }
        }
    } else {
        m_heroIdle->setText("--");
        m_stationSummary->setText("加载失败");
        m_stationLines[0]->setText("站点信息暂不可用\n点击卡片进入站点页重试");
        for (int i = 1; i < m_stationLines.size(); ++i) m_stationLines[i]->clear();
    }

    const QJsonObject activeReply = TcpClient::instance().request(
        Protocol::ReqUnfinishedOrder,
        QJsonObject{{"userId", ClientSession::instance().userId}}, 1600);
    if (activeReply.value("ok").toBool() && activeReply.value("hasOrder").toBool()) {
        const OrderInfo order = OrderInfo::fromJson(activeReply.value("order").toObject());
        m_chargeStatus->setText("充电中");
        m_chargeTitle->setText(QString("%1 · %2")
            .arg(order.stationName.isEmpty() ? QStringLiteral("当前充电站") : order.stationName,
                 order.pileCode.isEmpty() ? QStringLiteral("电桩信息更新中") : order.pileCode));
        m_chargeEnergy->setText(QString("%1 kWh").arg(order.energy, 0, 'f', 2));
        m_chargeAmount->setText(QString("¥ %1").arg(order.amount, 0, 'f', 2));
        m_chargeMinutes->setText(QString("%1 分钟").arg(order.simMinutes));
    } else {
        m_chargeStatus->setText("等待开始");
        m_chargeTitle->setText("当前没有进行中的充电任务");
        m_chargeEnergy->setText("-- kWh");
        m_chargeAmount->setText("¥ --");
        m_chargeMinutes->setText("-- 分钟");
    }

    const QJsonObject orderReply = TcpClient::instance().request(
        Protocol::ReqOrderHistory,
        QJsonObject{{"userId", ClientSession::instance().userId}, {"page", 0}, {"pageSize", 2}}, 1600);
    if (orderReply.value("ok").toBool()) {
        const QJsonArray orders = orderReply.value("orders").toArray();
        m_orderSummary->setText(QString("共 %1 笔").arg(orderReply.value("total").toInt(orders.size())));
        for (int i = 0; i < m_orderLines.size(); ++i) {
            if (i < orders.size()) {
                const OrderInfo order = OrderInfo::fromJson(orders[i].toObject());
                m_orderLines[i]->setText(QString("%1 · %2\n%3 kWh · ¥%4 · %5")
                    .arg(order.stationName.isEmpty() ? QStringLiteral("充电站") : order.stationName,
                         order.pileCode.isEmpty() ? QStringLiteral("--") : order.pileCode)
                    .arg(order.energy, 0, 'f', 2).arg(order.amount, 0, 'f', 2)
                    .arg(orderStatusText(order.status)));
            } else {
                m_orderLines[i]->setText(i == 0 ? "暂无订单记录\n完成充电后会生成旅程票据" : "");
            }
        }
    } else {
        m_orderSummary->setText("加载失败");
        m_orderLines[0]->setText("订单信息暂不可用\n点击卡片进入订单页重试");
        m_orderLines[1]->clear();
    }

    m_pageSummary->setText(QString("%1 个空闲桩 · %2 条未读消息 · 服务端 %3")
        .arg(m_heroIdle->text())
        .arg(MessageCenter::instance().unreadCount())
        .arg(TcpClient::instance().isConnected() ? "已连接" : "连接待确认"));
    m_loading = false;
    m_loaded = true;
}

void HomePage::refreshPage()
{
    m_loaded = false;
    loadNetworkDetails();
}

void HomePage::relayoutCards(int availableWidth)
{
    if (!m_grid || m_cards.size() != 7) return;
    const int columns = availableWidth < 820 ? 2 : 3;
    if (columns == m_layoutColumns) return;
    m_layoutColumns = columns;

    while (QLayoutItem *item = m_grid->takeAt(0))
        delete item;
    for (int column = 0; column < 3; ++column)
        m_grid->setColumnStretch(column, column < columns ? 1 : 0);
    for (int row = 0; row < 5; ++row)
        m_grid->setRowMinimumHeight(row, 0);

    if (columns == 3) {
        m_grid->addWidget(m_cards[0], 0, 0, 1, 2);
        m_grid->addWidget(m_cards[1], 0, 2);
        m_grid->addWidget(m_cards[2], 1, 0, 1, 2);
        m_grid->addWidget(m_cards[3], 1, 2);
        m_grid->addWidget(m_cards[4], 2, 0);
        m_grid->addWidget(m_cards[5], 2, 1);
        m_grid->addWidget(m_cards[6], 2, 2);
        m_grid->setRowMinimumHeight(0, 176);
        m_grid->setRowMinimumHeight(1, 220);
        m_grid->setRowMinimumHeight(2, 254);
    } else {
        m_grid->addWidget(m_cards[0], 0, 0, 1, 2);
        m_grid->addWidget(m_cards[2], 1, 0, 1, 2);
        m_grid->addWidget(m_cards[1], 2, 0);
        m_grid->addWidget(m_cards[3], 2, 1);
        m_grid->addWidget(m_cards[4], 3, 0);
        m_grid->addWidget(m_cards[5], 3, 1);
        m_grid->addWidget(m_cards[6], 4, 0, 1, 2);
        m_grid->setRowMinimumHeight(0, 176);
        m_grid->setRowMinimumHeight(1, 220);
        m_grid->setRowMinimumHeight(2, 220);
        m_grid->setRowMinimumHeight(3, 254);
        m_grid->setRowMinimumHeight(4, 220);
    }
}

void HomePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshLocalDetails();
    if (!m_loaded)
        QTimer::singleShot(0, this, &HomePage::loadNetworkDetails);
}

void HomePage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_loaded = false;
}

void HomePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutCards(event->size().width());
}
