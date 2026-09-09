#include "HomePage.h"

#include "ClientSession.h"
#include "IconFactory.h"
#include "MessageCenter.h"
#include "NavigationPage.h"
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
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTime>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QtMath>

class HomePowerGauge : public QWidget
{
public:
    explicit HomePowerGauge(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("homePowerGauge");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setMinimumSize(150, 132);
        m_animation.setDuration(520);
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) {
            const qreal progress = value.toReal();
            m_displayPower = m_fromPower + (m_toPower - m_fromPower) * progress;
            update();
        });
    }

    void setPower(double power)
    {
        const double normalized = power < 0 ? 0.0 : qMin(power, m_maxPower);
        m_active = power >= 0;
        m_animation.stop();
        m_fromPower = m_displayPower;
        m_toPower = normalized;
        m_animation.setStartValue(0.0);
        m_animation.setEndValue(1.0);
        m_animation.start();
        update();
    }

    double power() const { return m_active ? m_toPower : -1.0; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF center(width() / 2.0, height() * 0.72);
        const qreal radius = qMin(width() * 0.39, height() * 0.60);
        const QRectF arc(center.x() - radius, center.y() - radius,
                         radius * 2.0, radius * 2.0);
        constexpr int startAngle = 225 * 16;
        constexpr int spanAngle = -270 * 16;

        painter.setPen(QPen(QColor("#DDE8E3"), 12, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arc, startAngle, spanAngle);

        const qreal fraction = qBound(0.0, m_displayPower / m_maxPower, 1.0);
        QConicalGradient gradient(center, 225);
        gradient.setColorAt(0.00, QColor("#3ECF8E"));
        gradient.setColorAt(0.56, QColor("#35A7A0"));
        gradient.setColorAt(1.00, QColor("#4C7FD1"));
        painter.setPen(QPen(QBrush(gradient), 12, Qt::SolidLine, Qt::RoundCap));
        if (m_active)
            painter.drawArc(arc, startAngle, qRound(spanAngle * fraction));

        const qreal degrees = 225.0 - 270.0 * fraction;
        const qreal radians = qDegreesToRadians(degrees);
        const QPointF needle(center.x() + qCos(radians) * radius * 0.73,
                             center.y() - qSin(radians) * radius * 0.73);
        painter.setPen(QPen(QColor(m_active ? "#244A4A" : "#9AACAA"), 3,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(center, needle);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#244A4A"));
        painter.drawEllipse(center, 5.5, 5.5);

        painter.setPen(QColor("#82938F"));
        painter.setFont(QFont(QString(), 8, QFont::Medium));
        painter.drawText(QRectF(0, height() - 24, width(), 18), Qt::AlignCenter,
                         QStringLiteral("0                         150 kW"));
    }

private:
    QVariantAnimation m_animation;
    double m_displayPower = 0.0;
    double m_fromPower = 0.0;
    double m_toPower = 0.0;
    const double m_maxPower = 150.0;
    bool m_active = false;
};

namespace {
class FloatingPng : public QWidget
{
public:
    explicit FloatingPng(const QString &resourcePath, QWidget *parent = nullptr)
        : QWidget(parent), m_pixmap(resourcePath)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedSize(58, 58);
        m_animation.setDuration(2400);
        m_animation.setStartValue(0.0);
        m_animation.setKeyValueAt(0.5, 1.0);
        m_animation.setEndValue(0.0);
        m_animation.setEasingCurve(QEasingCurve::InOutSine);
        m_animation.setLoopCount(-1);
        connect(&m_animation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) { m_phase = value.toReal(); update(); });
        m_animation.start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setOpacity(0.72 + 0.18 * m_phase);
        painter.translate(0, 4.0 - 7.0 * m_phase);
        painter.drawPixmap(rect().adjusted(5, 5, -5, -5), m_pixmap);
    }

    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        m_animation.start();
    }

    void hideEvent(QHideEvent *event) override
    {
        QWidget::hideEvent(event);
        m_animation.stop();
    }

private:
    QPixmap m_pixmap;
    QVariantAnimation m_animation;
    qreal m_phase = 0.0;
};

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
    (*value)->setWordWrap(false);
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
    case OrderWaiting: return QStringLiteral("等待中");
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

    m_mapCard = new QFrame(this);
    m_mapCard->setObjectName("homeMapCard");
    m_mapCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *mapBody = new QVBoxLayout(m_mapCard);
    mapBody->setContentsMargins(16, 15, 16, 16);
    mapBody->setSpacing(10);
    auto *mapTop = new QHBoxLayout;
    auto *mapIcon = new QLabel(m_mapCard);
    mapIcon->setObjectName("homeMapIcon");
    mapIcon->setAlignment(Qt::AlignCenter);
    mapIcon->setPixmap(IconFactory::icon(IconFactory::IconLocation,
                                         QColor("#427B72"), 24).pixmap(24, 24));
    mapIcon->setFixedSize(40, 40);
    auto *mapHeading = new QVBoxLayout;
    mapHeading->setSpacing(1);
    auto *mapTitle = new QLabel("附近充电地图", m_mapCard);
    mapTitle->setObjectName("homeMapTitle");
    auto *mapSubtitle = new QLabel("拖动、缩放并点击标记查看站点", m_mapCard);
    mapSubtitle->setObjectName("homeMapSubtitle");
    mapHeading->addWidget(mapTitle);
    mapHeading->addWidget(mapSubtitle);
    m_mapSummary = new QLabel("正在载入站点", m_mapCard);
    m_mapSummary->setObjectName("homeMapBadge");
    auto *mapAction = new QPushButton("查看完整地图  ↗", m_mapCard);
    mapAction->setObjectName("homeMapAction");
    mapAction->setCursor(Qt::PointingHandCursor);
    connect(mapAction, &QPushButton::clicked, this, [this] { emit pageRequested(1); });
    auto *carAsset = new FloatingPng(":/icons/noto-emoji/electric-car.png", m_mapCard);
    carAsset->setFixedSize(48, 48);
    mapTop->addWidget(mapIcon);
    mapTop->addLayout(mapHeading, 1);
    mapTop->addWidget(carAsset, 0, Qt::AlignVCenter);
    mapTop->addWidget(m_mapSummary, 0, Qt::AlignVCenter);
    mapTop->addWidget(mapAction, 0, Qt::AlignVCenter);
    mapBody->addLayout(mapTop);
    m_mapCanvas = new MapCanvas(m_mapCard);
    m_mapCanvas->setObjectName("homeMapCanvas");
    m_mapCanvas->setMinimumHeight(245);
    m_mapCanvas->setData({}, 116.3100, 39.9600, -1, {});
    mapBody->addWidget(m_mapCanvas, 1);

    auto *power = createBaseCard("实时充电功率", "充电时同步展示功率变化",
                                 IconFactory::IconBolt, "power", "meter", 2,
                                 &m_chargeStatus);
    auto *powerBody = cardBody(power);
    auto *meterRow = new QHBoxLayout;
    meterRow->setSpacing(6);
    m_powerGauge = new HomePowerGauge(power);
    meterRow->addWidget(m_powerGauge, 1);
    auto *powerCopy = new QVBoxLayout;
    powerCopy->setSpacing(2);
    m_powerValue = detailLabel("-- kW", power, "homePowerValue");
    m_powerValue->setAlignment(Qt::AlignCenter);
    m_powerHint = detailLabel("等待充电任务", power, "homePowerHint");
    m_powerHint->setAlignment(Qt::AlignCenter);
    auto *plugAsset = new FloatingPng(":/icons/noto-emoji/electric-plug.png", power);
    powerCopy->addWidget(plugAsset, 0, Qt::AlignHCenter);
    powerCopy->addWidget(m_powerValue);
    powerCopy->addWidget(m_powerHint);
    powerCopy->addStretch();
    meterRow->addLayout(powerCopy);
    powerBody->addLayout(meterRow, 1);
    m_chargeTitle = detailLabel("当前没有进行中的充电任务", power, "homeFeatureTitle");
    powerBody->addWidget(m_chargeTitle);
    auto *chargeStats = new QHBoxLayout;
    chargeStats->setSpacing(7);
    chargeStats->addWidget(statBlock("电量", &m_chargeEnergy, power));
    chargeStats->addWidget(statBlock("费用", &m_chargeAmount, power));
    chargeStats->addWidget(statBlock("时长", &m_chargeMinutes, power));
    powerBody->addLayout(chargeStats);
    addAction(power, "进入充电服务  →");

    auto *hero = createBaseCard("今日补能概览",
                                "账户、站点与服务连接，一眼掌握",
                                IconFactory::IconPlug, "hero", "wide", 1,
                                &m_connectionStatus);
    auto *heroBody = cardBody(hero);
    auto *heroStats = new QHBoxLayout;
    heroStats->setSpacing(10);
    heroStats->addWidget(statBlock("账户余额", &m_heroBalance, hero));
    heroStats->addWidget(statBlock("附近空闲桩", &m_heroIdle, hero));
    heroStats->addWidget(statBlock("未读消息", &m_heroUnread, hero));
    heroBody->addLayout(heroStats);
    m_endpoint = detailLabel("服务端 --:--", hero, "homeEndpointInline");
    heroBody->addWidget(m_endpoint);
    addAction(hero, "查看附近充电站  →");

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
    m_cards = {m_mapCard, power, hero, account, stations, orders, messages};
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
        const int type = message.value("type").toInt();
        if (type == Protocol::PushOrderProgress) {
            const double power = message.value("power").toDouble(-1);
            const int minutes = message.value("minutes").toInt();
            const int orderId = message.value("orderId").toInt();
            m_chargeStatus->setText("充电中");
            m_chargeTitle->setText(orderId > 0
                ? QString("功率数据实时更新 · 订单 #%1").arg(orderId)
                : QStringLiteral("功率数据正在实时更新"));
            m_chargeEnergy->setText(QString("%1 kWh").arg(message.value("energy").toDouble(), 0, 'f', 1));
            m_chargeAmount->setText(QString("¥%1").arg(message.value("amount").toDouble(), 0, 'f', 2));
            m_chargeMinutes->setText(QString("%1 分").arg(minutes));
            if (power >= 0) {
                m_powerGauge->setPower(power);
                m_powerValue->setText(QString("%1 kW").arg(power, 0, 'f', 1));
                m_powerHint->setText("实时功率");
            }
            return;
        }
        if (type == Protocol::PushOrderEvent
            && (message.value("event").toInt() == 2
                || message.value("event").toInt() == 3)) {
            m_powerGauge->setPower(-1);
            m_powerValue->setText("-- kW");
            m_powerHint->setText("充电已结束");
            m_chargeStatus->setText("已结束");
        }
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

    m_endpoint->setText(QString("服务端 %1:%2 · %3")
        .arg(TcpClient::instance().serverHost())
        .arg(TcpClient::instance().serverPort())
        .arg(TcpClient::instance().isConnected() ? "连接正常" : "等待重连"));
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
        m_stationData.clear();
        int idle = 0;
        for (const QJsonValue &value : stations) {
            const StationInfo station = StationInfo::fromJson(value.toObject());
            m_stationData.append(station);
            idle += station.idlePiles;
        }
        m_mapCanvas->setData(m_stationData, 116.3100, 39.9600, -1, {});
        m_mapSummary->setText(QString("%1 站 · %2 空闲").arg(stations.size()).arg(idle));
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
        m_stationData.clear();
        m_mapCanvas->setData({}, 116.3100, 39.9600, -1, {});
        m_mapSummary->setText("地图数据待重试");
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
        m_chargeEnergy->setText(QString("%1 kWh").arg(order.energy, 0, 'f', 1));
        m_chargeAmount->setText(QString("¥%1").arg(order.amount, 0, 'f', 2));
        m_chargeMinutes->setText(QString("%1 分").arg(order.simMinutes));
        if (m_powerGauge->power() < 0)
            m_powerHint->setText("等待下一次功率推送");
    } else {
        m_chargeStatus->setText("等待开始");
        m_chargeTitle->setText("当前没有进行中的充电任务");
        m_chargeEnergy->setText("-- kWh");
        m_chargeAmount->setText("¥--");
        m_chargeMinutes->setText("-- 分");
        m_powerGauge->setPower(-1);
        m_powerValue->setText("-- kW");
        m_powerHint->setText("等待充电任务");
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
        m_grid->setRowMinimumHeight(0, 350);
        m_grid->setRowMinimumHeight(1, 190);
        m_grid->setRowMinimumHeight(2, 254);
    } else {
        m_grid->addWidget(m_cards[0], 0, 0, 1, 2);
        m_grid->addWidget(m_cards[1], 1, 0, 1, 2);
        m_grid->addWidget(m_cards[2], 2, 0, 1, 2);
        m_grid->addWidget(m_cards[3], 3, 0);
        m_grid->addWidget(m_cards[4], 3, 1);
        m_grid->addWidget(m_cards[5], 4, 0);
        m_grid->addWidget(m_cards[6], 4, 1);
        m_grid->setRowMinimumHeight(0, 330);
        m_grid->setRowMinimumHeight(1, 330);
        m_grid->setRowMinimumHeight(2, 190);
        m_grid->setRowMinimumHeight(3, 254);
        m_grid->setRowMinimumHeight(4, 254);
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
