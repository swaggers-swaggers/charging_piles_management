#include "SalesPage.h"

#include "OrderDao.h"
#include "PileDao.h"
#include "ServerSession.h"
#include "StationDao.h"
#include "UserDao.h"
#include "IconFactory.h"

#include <QAbstractSocket>
#include <QClipboard>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QNetworkInterface>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QTcpServer>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

class RevenueChart : public QWidget
{
public:
    explicit RevenueChart(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("adminRevenueChart"));
        setMinimumHeight(230);
    }

    void setData(const QVector<QPair<QString, double>> &data) { m_data = data; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF plot = QRectF(rect()).adjusted(46, 14, -16, -30);
        if (m_data.isEmpty() || plot.width() <= 0 || plot.height() <= 0) return;
        double maximum = 1.0;
        for (const auto &entry : m_data) maximum = qMax(maximum, entry.second);
        p.setPen(QPen(QColor("#DDEAE2"), 1));
        for (int i = 0; i <= 4; ++i) {
            const qreal y = plot.top() + plot.height() * i / 4.0;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            p.setPen(QColor("#85968D"));
            p.drawText(QRectF(0, y - 9, 40, 18), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(maximum * (4 - i) / 4.0, 'f', 0));
            p.setPen(QPen(QColor("#DDEAE2"), 1));
        }
        const int count = m_data.size();
        const int labelStep = qMax(1, count / 6);
        const qreal barWidth = qMin(26.0, plot.width() * 0.48 / qMax(1, count));
        QPainterPath line, area;
        for (int i = 0; i < count; ++i) {
            const qreal x = plot.left() + plot.width() * (i + 0.5) / count;
            const qreal y = plot.bottom() - plot.height() * m_data[i].second / maximum;
            if (i == 0) { line.moveTo(x, y); area.moveTo(x, plot.bottom()); area.lineTo(x, y); }
            else { line.lineTo(x, y); area.lineTo(x, y); }
            p.setPen(Qt::NoPen); p.setBrush(QColor(114, 174, 139, 62));
            p.drawRoundedRect(QRectF(x - barWidth / 2, y, barWidth,
                                     qMax(2.0, plot.bottom() - y)), 3, 3);
            if (i % labelStep == 0 || i == count - 1) {
                p.setPen(QColor("#82938A"));
                p.drawText(QRectF(x - 30, plot.bottom() + 6, 60, 18),
                           Qt::AlignHCenter | Qt::AlignTop, m_data[i].first.mid(5));
            }
        }
        area.lineTo(plot.left() + plot.width() * (count - 0.5) / count, plot.bottom());
        area.closeSubpath();
        QLinearGradient fill(plot.topLeft(), plot.bottomLeft());
        fill.setColorAt(0.0, QColor(65, 151, 111, 92));
        fill.setColorAt(1.0, QColor(65, 151, 111, 4));
        p.setBrush(fill); p.setPen(Qt::NoPen); p.drawPath(area);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#2E8860"), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(line);
    }
private:
    QVector<QPair<QString, double>> m_data;
};

namespace {
QFrame *card(const QString &name, QWidget *parent)
{
    auto *c = new QFrame(parent); c->setObjectName(name); c->setProperty("adminHomeCard", true);
    c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); return c;
}
QLabel *text(const QString &value, const char *name, QWidget *parent)
{
    auto *l = new QLabel(value, parent); l->setObjectName(name); l->setWordWrap(true); return l;
}
QPushButton *action(const QString &value, QWidget *parent)
{
    auto *b = new QPushButton(value, parent); b->setObjectName("adminHomeAction");
    b->setCursor(Qt::PointingHandCursor); return b;
}
QFrame *stat(const QString &caption, QLabel **value, QWidget *parent)
{
    auto *block = card("adminHomeStat", parent); auto *box = new QVBoxLayout(block);
    box->setContentsMargins(12, 9, 12, 9); box->setSpacing(2);
    *value = text("--", "adminHomeStatValue", block); box->addWidget(*value);
    box->addWidget(text(caption, "adminHomeStatCaption", block)); return block;
}
QLabel *dataIcon(IconFactory::IconType type, QWidget *parent)
{
    auto *icon = new QLabel(parent); icon->setObjectName("adminHomeDataIcon");
    icon->setAlignment(Qt::AlignCenter); icon->setFixedSize(30, 30);
    icon->setPixmap(IconFactory::icon(type, QColor("#357759"), 16).pixmap(16, 16));
    return icon;
}
QFrame *dataPill(IconFactory::IconType type, const QString &caption,
                 QLabel **value, QWidget *parent)
{
    auto *pill = new QFrame(parent); pill->setObjectName("adminHomeDataPill");
    auto *row = new QHBoxLayout(pill); row->setContentsMargins(8, 6, 9, 6); row->setSpacing(7);
    row->addWidget(dataIcon(type, pill));
    auto *copy = new QVBoxLayout; copy->setSpacing(0);
    *value = text("--", "adminHomeDataValue", pill);
    copy->addWidget(*value); copy->addWidget(text(caption, "adminHomeDataCaption", pill));
    row->addLayout(copy, 1); return pill;
}
}

SalesPage::SalesPage(QWidget *parent) : QWidget(parent)
{
    setObjectName("adminHomePage");
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this); scroll->setObjectName("adminHomeScroll");
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll); content->setObjectName("adminHomeContent");
    auto *page = new QVBoxLayout(content); page->setContentsMargins(28, 24, 28, 32); page->setSpacing(18);

    auto *welcome = new QHBoxLayout; auto *welcomeCopy = new QVBoxLayout;
    m_adminName = text({}, "adminHomeGreeting", content);
    welcomeCopy->addWidget(m_adminName);
    welcomeCopy->addWidget(text("营收、设备与连接状态，集中在一个首页", "adminHomeIntro", content));
    auto *refreshButton = action("刷新首页", content);
    welcome->addLayout(welcomeCopy, 1); welcome->addWidget(refreshButton, 0, Qt::AlignVCenter);
    page->addLayout(welcome); connect(refreshButton, &QPushButton::clicked, this, &SalesPage::refresh);

    auto *grid = new QGridLayout; grid->setHorizontalSpacing(16); grid->setVerticalSpacing(16);
    grid->setColumnStretch(0, 13); grid->setColumnStretch(1, 17); grid->setColumnStretch(2, 12);

    auto *hero = card("adminHomeHero", content); auto *heroBox = new QVBoxLayout(hero);
    heroBox->setContentsMargins(22, 18, 22, 18);
    auto *heroHead = new QHBoxLayout; heroHead->addWidget(text("今日运营概览", "adminHomeCardTitle", hero));
    heroHead->addStretch(); m_serverSummary = text("服务正在初始化", "adminHomeBadge", hero);
    heroHead->addWidget(m_serverSummary); heroBox->addLayout(heroHead);
    auto *metrics = new QHBoxLayout; metrics->setSpacing(10);
    metrics->addWidget(stat("今日营收 / 元", &m_todayVal, hero));
    metrics->addWidget(stat("近 30 日 / 元", &m_monthVal, hero));
    metrics->addWidget(stat("累计营收 / 元", &m_totalVal, hero)); heroBox->addLayout(metrics);
    auto *ordersAction = action("查看订单  →", hero); heroBox->addWidget(ordersAction, 0, Qt::AlignRight);
    connect(ordersAction, &QPushButton::clicked, this, [this] { emit pageRequested(3); });

    auto *connection = card("adminConnectionCard", content); auto *connectionBox = new QVBoxLayout(connection);
    connectionBox->setContentsMargins(18, 17, 18, 17); connectionBox->setSpacing(10);
    auto *connectionHead = new QHBoxLayout;
    connectionHead->addWidget(text("连接客户端", "adminHomeCardTitle", connection));
    connectionHead->addStretch();
    m_lanStatus = text("检测中", "lanStatusBadge", connection);
    connectionHead->addWidget(m_lanStatus, 0, Qt::AlignVCenter);
    connectionBox->addLayout(connectionHead);
    connectionBox->addWidget(text("复制推荐地址，填入客户端登录页的服务器地址", "adminHomeCardHint", connection));
    auto *panel = new QWidget(connection); panel->setObjectName("lanConnectionPanel");
    auto *panelBox = new QVBoxLayout(panel); panelBox->setContentsMargins(0, 0, 0, 0); panelBox->setSpacing(8);

    auto *primaryPanel = new QFrame(panel); primaryPanel->setObjectName("lanPrimaryAddressPanel");
    auto *primaryBox = new QVBoxLayout(primaryPanel); primaryBox->setContentsMargins(14, 11, 14, 11); primaryBox->setSpacing(4);
    primaryBox->addWidget(text("推荐 · 局域网", "lanAddressEyebrow", primaryPanel));
    m_addressesText = text("--", "lanAddressText", primaryPanel);
    m_addressesText->setAccessibleName("服务器 IP 与端口");
    m_addressesText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    primaryBox->addWidget(m_addressesText);
    m_addressMeta = text("正在检测可用网络", "lanAddressMeta", primaryPanel);
    primaryBox->addWidget(m_addressMeta);
    auto *addressRow = new QHBoxLayout;
    m_copyAddressButton = action("复制推荐地址", primaryPanel);
    auto *addressRefresh = action("重新检测", primaryPanel);
    addressRow->addWidget(m_copyAddressButton); addressRow->addWidget(addressRefresh); addressRow->addStretch();
    primaryBox->addLayout(addressRow);
    panelBox->addWidget(primaryPanel);

    m_otherAddresses = text({}, "lanOtherAddresses", panel);
    m_otherAddresses->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_otherAddresses->hide();
    panelBox->addWidget(m_otherAddresses);

    auto *localPanel = new QFrame(panel); localPanel->setObjectName("lanLocalAddressPanel");
    auto *localRow = new QHBoxLayout(localPanel); localRow->setContentsMargins(11, 8, 11, 8);
    localRow->addWidget(text("仅本机调试", "lanLocalCaption", localPanel)); localRow->addStretch();
    m_localAddress = text("--", "lanLocalAddress", localPanel);
    m_localAddress->setTextInteractionFlags(Qt::TextSelectableByMouse);
    localRow->addWidget(m_localAddress);
    panelBox->addWidget(localPanel);
    connectionBox->addWidget(panel);

    auto *webRow = new QHBoxLayout;
    m_webStatus = text("数据大屏地址待确认", "adminHomeEndpoint", connection);
    webRow->addWidget(m_webStatus, 1);
    auto *webButton = action("打开数据大屏  ↗", connection); webButton->setObjectName("openWebBtn");
    webRow->addWidget(webButton); connectionBox->addLayout(webRow);
    connect(m_copyAddressButton, &QPushButton::clicked, this, [this] {
        if (!m_primaryEndpoint.isEmpty()) QGuiApplication::clipboard()->setText(m_primaryEndpoint);
    });
    connect(addressRefresh, &QPushButton::clicked, this, &SalesPage::refreshConnectionInfo);
    connect(webButton, &QPushButton::clicked, this, &SalesPage::openWebRequested);

    auto *trend = card("adminTrendCard", content); auto *trendBox = new QVBoxLayout(trend);
    trendBox->setContentsMargins(18, 16, 18, 16); auto *trendHead = new QHBoxLayout;
    trendHead->addWidget(text("营收趋势", "adminHomeCardTitle", trend)); trendHead->addStretch();
    m_rangeCombo = new QComboBox(trend); m_rangeCombo->setObjectName("rangeCombo");
    m_rangeCombo->addItems({"近 7 日", "近 30 日"}); trendHead->addWidget(m_rangeCombo);
    trendBox->addLayout(trendHead); m_chart = new RevenueChart(trend); trendBox->addWidget(m_chart, 1);
    connect(m_rangeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &SalesPage::refresh);

    auto *pile = card("adminPileCard", content); auto *pileBox = new QVBoxLayout(pile);
    pileBox->setContentsMargins(18, 16, 18, 16); pileBox->addWidget(text("电桩实时状态", "adminHomeCardTitle", pile));
    auto *pileStats = new QHBoxLayout; pileStats->addWidget(stat("空闲", &m_pileIdle, pile));
    pileStats->addWidget(stat("充电中", &m_pileBusy, pile)); pileStats->addWidget(stat("故障", &m_pileFault, pile));
    pileBox->addLayout(pileStats); auto *pileAction = action("进入电桩状态  →", pile);
    pileBox->addWidget(pileAction, 0, Qt::AlignRight);
    connect(pileAction, &QPushButton::clicked, this, [this] { emit pageRequested(1); });

    auto *station = card("adminStationCard", content); auto *stationBox = new QVBoxLayout(station);
    stationBox->setContentsMargins(18, 16, 18, 16); stationBox->setSpacing(8);
    auto *stationHead = new QHBoxLayout; stationHead->addWidget(dataIcon(IconFactory::IconBuilding, station));
    stationHead->addWidget(text("站点网络", "adminHomeCardTitle", station)); stationHead->addStretch();
    stationBox->addLayout(stationHead);
    m_stationCount = text("--", "adminHomeBigValue", station); stationBox->addWidget(m_stationCount);
    stationBox->addWidget(dataPill(IconFactory::IconPile, "电桩资源", &m_stationPileDetail, station));
    stationBox->addWidget(dataPill(IconFactory::IconBolt, "平均电价", &m_stationPriceDetail, station));
    stationBox->addWidget(dataPill(IconFactory::IconChartLine, "营收领先", &m_stationLeader, station));
    stationBox->addStretch();
    auto *stationAction = action("管理站点与电桩  →", station); stationBox->addWidget(stationAction, 0, Qt::AlignRight);
    connect(stationAction, &QPushButton::clicked, this, [this] { emit pageRequested(2); });

    auto *orders = card("adminOrderCard", content); auto *orderBox = new QHBoxLayout(orders);
    orderBox->setContentsMargins(20, 16, 20, 16); orderBox->setSpacing(10); auto *orderCopy = new QVBoxLayout;
    auto *orderHead = new QHBoxLayout; orderHead->addWidget(dataIcon(IconFactory::IconChartLine, orders));
    orderHead->addWidget(text("订单脉搏", "adminHomeCardTitle", orders)); orderHead->addStretch();
    orderCopy->addLayout(orderHead);
    m_orderCount = text("--", "adminHomeBigValue", orders); orderCopy->addWidget(m_orderCount); orderCopy->addStretch();
    orderBox->addLayout(orderCopy, 2);
    orderBox->addWidget(dataPill(IconFactory::IconBolt, "正在充电", &m_orderActive, orders), 1);
    orderBox->addWidget(dataPill(IconFactory::IconBattery, "已完成", &m_orderCompleted, orders), 1);
    orderBox->addWidget(dataPill(IconFactory::IconPlug, "累计电量", &m_orderEnergy, orders), 1);
    auto *orderAction = action("处理订单  →", orders); orderBox->addWidget(orderAction, 0, Qt::AlignBottom);
    connect(orderAction, &QPushButton::clicked, this, [this] { emit pageRequested(3); });

    auto *users = card("adminUserCard", content); auto *userBox = new QHBoxLayout(users);
    userBox->setContentsMargins(18, 14, 18, 14); userBox->setSpacing(10);
    auto *userCopy = new QVBoxLayout; auto *userHead = new QHBoxLayout;
    userHead->addWidget(dataIcon(IconFactory::IconUsers, users));
    userHead->addWidget(text("车主用户", "adminHomeCardTitle", users)); userHead->addStretch();
    userCopy->addLayout(userHead); m_userCount = text("--", "adminHomeBigValue", users);
    userCopy->addWidget(m_userCount); userBox->addLayout(userCopy, 2);
    userBox->addWidget(dataPill(IconFactory::IconUser, "账户状态", &m_userStatus, users), 1);
    userBox->addWidget(dataPill(IconFactory::IconBolt, "账户总余额", &m_userBalance, users), 1);
    userBox->addWidget(dataPill(IconFactory::IconUsers, "最近注册", &m_userNewest, users), 1);
    auto *userAction = action("查看用户  →", users); userBox->addWidget(userAction, 0, Qt::AlignBottom);
    connect(userAction, &QPushButton::clicked, this, [this] { emit pageRequested(4); });

    grid->addWidget(hero, 0, 0, 1, 2); grid->addWidget(connection, 0, 2, 2, 1);
    grid->addWidget(trend, 1, 0, 2, 2); grid->addWidget(pile, 2, 2);
    grid->addWidget(station, 3, 0, 2, 1); grid->addWidget(orders, 3, 1, 1, 2);
    grid->addWidget(users, 4, 1, 1, 2);
    grid->setRowMinimumHeight(0, 210); grid->setRowMinimumHeight(1, 150);
    grid->setRowMinimumHeight(2, 190); grid->setRowMinimumHeight(3, 170); grid->setRowMinimumHeight(4, 145);
    page->addLayout(grid);
    page->addWidget(text("数据自动刷新 · 卡片中的入口直达对应业务", "adminHomeFootnote", content), 0, Qt::AlignCenter);
    scroll->setWidget(content); outer->addWidget(scroll);

    m_connectionTimer = new QTimer(this); m_connectionTimer->setInterval(10000);
    connect(m_connectionTimer, &QTimer::timeout, this, &SalesPage::refreshConnectionInfo);
    m_connectionTimer->start(); refresh();
}

void SalesPage::showConnectionInfo(QTcpServer *server, const QString &serverInfo, const QString &webUrl)
{
    m_tcpServer = server; m_webUrl = webUrl;
    m_serverSummary->setText(serverInfo.contains("监听中") ? "服务在线" : "服务待检查");
    m_webStatus->setText(webUrl.isEmpty() ? "数据大屏未启动" : QString("大屏 %1").arg(webUrl));
    refreshConnectionInfo();
}

void SalesPage::refreshConnectionInfo()
{
    m_primaryEndpoint.clear();
    if (!m_tcpServer || !m_tcpServer->isListening()) {
        m_lanStatus->setText("未启动");
        m_addressesText->setText("--");
        m_addressMeta->setText(m_tcpServer ? "监听失败 · " + m_tcpServer->errorString() : "服务启动后自动显示连接地址");
        m_localAddress->setText("--");
        m_otherAddresses->hide();
        m_copyAddressButton->setEnabled(false);
        return;
    }
    QStringList seen;
    QString primaryInterface;
    QStringList otherAddresses;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback() || ip.isNull()
                || ip.isLinkLocal() || seen.contains(ip.toString())) continue;
            seen.append(ip.toString()); const QString endpoint = QString("%1:%2").arg(ip.toString()).arg(m_tcpServer->serverPort());
            if (m_primaryEndpoint.isEmpty()) {
                m_primaryEndpoint = endpoint;
                primaryInterface = iface.humanReadableName();
            } else {
                otherAddresses.append(QString("%1   %2").arg(endpoint, iface.humanReadableName()));
            }
        }
    }
    const QString local = QString("127.0.0.1:%1").arg(m_tcpServer->serverPort());
    m_localAddress->setText(local);
    if (m_primaryEndpoint.isEmpty()) {
        m_primaryEndpoint = local;
        m_lanStatus->setText("仅本机");
        m_addressesText->setText(local);
        m_addressMeta->setText("未检测到局域网地址 · 此地址只能在服务端电脑使用");
    } else {
        m_lanStatus->setText("可连接");
        m_addressesText->setText(m_primaryEndpoint);
        m_addressMeta->setText(QString("%1 · 客户端与服务端需连接同一网络").arg(primaryInterface));
    }
    m_otherAddresses->setText(otherAddresses.isEmpty()
        ? QString() : QString("其他可用网络\n%1").arg(otherAddresses.join('\n')));
    m_otherAddresses->setVisible(!otherAddresses.isEmpty());
    m_copyAddressButton->setEnabled(true);
}

void SalesPage::refreshPage() { refresh(); }

void SalesPage::refresh()
{
    const int hour = QTime::currentTime().hour();
    const QString greeting = hour < 11 ? "早上好" : hour < 14 ? "中午好" : hour < 18 ? "下午好" : "晚上好";
    m_adminName->setText(QString("%1，%2").arg(greeting, ServerSession::instance().adminName.isEmpty() ? "管理员" : ServerSession::instance().adminName));
    double today = 0, month = 0, total = 0;
    if (OrderDao::salesSummary(&today, &month, &total)) {
        m_todayVal->setText(QString::number(today, 'f', 2)); m_monthVal->setText(QString::number(month, 'f', 2));
        m_totalVal->setText(QString::number(total, 'f', 2));
    } else { m_todayVal->setText("--"); m_monthVal->setText("--"); m_totalVal->setText("--"); }
    int idle = 0, busy = 0, fault = 0; PileDao::statusCounts(&idle, &busy, &fault);
    m_pileIdle->setText(QString::number(idle)); m_pileBusy->setText(QString::number(busy)); m_pileFault->setText(QString::number(fault));
    const auto stations = StationDao::list(); m_stationCount->setText(QString("%1 个站点").arg(stations.size()));
    int stationPiles = 0, stationIdle = 0; double stationPrice = 0.0;
    for (const auto &station : stations) {
        stationPiles += station.totalPiles; stationIdle += station.idlePiles; stationPrice += station.price;
    }
    m_stationPileDetail->setText(QString("%1 桩 · %2 空闲").arg(stationPiles).arg(stationIdle));
    m_stationPriceDetail->setText(stations.isEmpty() ? "--" : QString("¥%1 / 度").arg(stationPrice / stations.size(), 0, 'f', 2));
    const auto ranking = OrderDao::stationRevenue();
    m_stationLeader->setText(ranking.isEmpty() ? "暂无营收"
        : QString("%1 · ¥%2").arg(ranking.first().first).arg(ranking.first().second, 0, 'f', 2));
    const auto orderList = OrderDao::listAll(-1); int active = 0, completed = 0; double orderEnergy = 0.0;
    for (const auto &order : orderList) {
        if (order.status == OrderCharging) ++active;
        if (order.status == OrderFinished) ++completed;
        orderEnergy += order.energy;
    }
    m_orderCount->setText(QString("%1 笔订单").arg(orderList.size()));
    m_orderActive->setText(QString("%1 笔").arg(active));
    m_orderCompleted->setText(QString("%1 笔").arg(completed));
    m_orderEnergy->setText(QString("%1 kWh").arg(orderEnergy, 0, 'f', 1));
    const auto users = UserDao::list(); int normalUsers = 0; double userBalance = 0.0;
    for (const auto &user : users) { if (user.status == UserNormal) ++normalUsers; userBalance += user.balance; }
    m_userCount->setText(QString("%1 位用户").arg(users.size()));
    m_userStatus->setText(QString("%1 正常 · %2 冻结").arg(normalUsers).arg(users.size() - normalUsers));
    m_userBalance->setText(QString("¥%1").arg(userBalance, 0, 'f', 2));
    if (users.isEmpty()) {
        m_userNewest->setText(QStringLiteral("暂无用户"));
    } else {
        const auto &newest = users.constLast();
        m_userNewest->setText(newest.nickname.trimmed().isEmpty() ? newest.phone : newest.nickname);
    }
    m_chart->setData(OrderDao::dailyRevenue(m_rangeCombo->currentIndex() == 1 ? 30 : 7));
    refreshConnectionInfo();
}
