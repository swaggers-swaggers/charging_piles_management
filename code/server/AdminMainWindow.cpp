#include "AdminTableCard.h"
#include "UiMotion.h"
#include "AdminMainWindow.h"

#include "DatabaseManager.h"
#include "ChargingEngine.h"
#include "ServerSession.h"
#include "SalesPage.h"
#include "PileStatusPage.h"
#include "OrderManagePage.h"
#include "StationManagePage.h"
#include "UserManagePage.h"
#include "IconFactory.h"

#include <QDesktopServices>
#include <QApplication>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QComboBox>
#include <QClipboard>
#include <QTimer>
#include <QGuiApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSize>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QUrl>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

AdminMainWindow::AdminMainWindow(const QString &serverInfo, const QString &webUrl, QWidget *parent)
    : QMainWindow(parent)
    , m_serverInfo(serverInfo)
    , m_webUrl(webUrl)
{
    setWindowTitle("东软电动汽车充电桩应用管理平台 - 服务端");
    // 尺寸自适应屏幕, 避免在分辨率较小的虚拟机窗口上超出屏幕看不到
    const QSize screen = QGuiApplication::primaryScreen()->availableGeometry().size();
    resize(qMin(1100, qMax(720, screen.width() - 80)),
           qMin(700, qMax(500, screen.height() - 120)));

    initUi();
    AdminTableCard::decorate(this);
    UiMotion::install(this);

    statusBar()->showMessage(QString("管理员: %1    |    数据库: %2    |    %3")
                                 .arg(ServerSession::instance().adminName,
                                      DatabaseManager::instance().databasePath(),
                                      m_serverInfo));
}

void AdminMainWindow::initUi()
{
    QWidget *central = new QWidget(this);
    central->setObjectName("appCentral");
    QHBoxLayout *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(central);

    // ---------- 左侧导航 ----------
    QWidget *sidebar = new QWidget(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(200);
    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 20, 0, 12);
    sideLayout->setSpacing(10);

    QWidget *logoBox = new QWidget(sidebar);
    logoBox->setObjectName("logoBox");
    logoBox->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *logoLayout = new QHBoxLayout(logoBox);
    logoLayout->setContentsMargins(0, 0, 0, 0);
    logoLayout->setSpacing(8);
    QLabel *logoIcon = new QLabel(logoBox);
    logoIcon->setPixmap(IconFactory::icon(IconFactory::IconBolt, QColor("#37C6FF")).pixmap(22, 22));
    QLabel *logo = new QLabel("充电桩管理平台", logoBox);
    logo->setObjectName("logoLabel");
    logoLayout->addStretch();
    logoLayout->addWidget(logoIcon);
    logoLayout->addWidget(logo);
    logoLayout->addStretch();

    m_navList = new QListWidget(sidebar);
    m_navList->setObjectName("navList");
    const QStringList navNames = {
        "销售业绩", "电桩状态", "充电站与电桩管理", "订单管理", "用户管理",
    };
    const QVector<IconFactory::IconType> navIcons = {
        IconFactory::IconChartLine, IconFactory::IconBattery, IconFactory::IconBuilding,
        IconFactory::IconBolt, IconFactory::IconUsers,
    };
    for (int i = 0; i < navNames.size(); ++i) {
        auto *item = new QListWidgetItem(navNames[i]);
        item->setIcon(IconFactory::icon(navIcons[i]));
        item->setData(Qt::UserRole, navNames[i]);   // 纯文本标题(不含图标)
        m_navList->addItem(item);
    }
    m_navList->setIconSize(QSize(20, 20));
    m_navList->setCurrentRow(0);
    m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QPushButton *logoutBtn = new QPushButton("退出登录", sidebar);
    logoutBtn->setObjectName("logoutBtn");
    logoutBtn->setCursor(Qt::PointingHandCursor);

    sideLayout->addWidget(logoBox);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(m_navList, 1);
    sideLayout->addWidget(logoutBtn);

    // ---------- 右侧: 页头 + 页面栈 ----------
    QWidget *rightArea = new QWidget(central);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QWidget *header = new QWidget(rightArea);
    header->setObjectName("headerBar");
    header->setFixedHeight(56);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 0, 24, 0);

    m_headerTitle = new QLabel(navNames.first(), header);
    m_headerTitle->setObjectName("headerTitle");
    m_headerUser = new QLabel("管理员: " + ServerSession::instance().adminName, header);
    m_headerUser->setObjectName("headerUser");

    QPushButton *openWebBtn = new QPushButton("打开大屏", header);
    openWebBtn->setObjectName("openWebBtn");
    openWebBtn->setCursor(Qt::PointingHandCursor);
    openWebBtn->setFixedHeight(34);

    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(openWebBtn);
    headerLayout->addWidget(m_headerUser);

    m_stack = new QStackedWidget(rightArea);
    m_stack->setObjectName("contentStack");
    m_stack->addWidget(new SalesPage());
    m_stack->addWidget(new PileStatusPage());
    m_stack->addWidget(new StationManagePage());
    m_stack->addWidget(new OrderManagePage());
    m_stack->addWidget(new UserManagePage());

    rightLayout->addWidget(header);
    rightLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(rightArea, 1);

    connect(m_navList, &QListWidget::currentRowChanged,
            this, &AdminMainWindow::onNavChanged);
    connect(logoutBtn, &QPushButton::clicked,
            this, &AdminMainWindow::onLogoutClicked);
    connect(openWebBtn, &QPushButton::clicked,
            this, &AdminMainWindow::onOpenWebClicked);

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setObjectName("pageAutoRefreshTimer");
    m_autoRefreshTimer->setInterval(5000);
    connect(m_autoRefreshTimer, &QTimer::timeout,
            this, &AdminMainWindow::refreshCurrentPage);
    connect(&ChargingEngine::instance(), &ChargingEngine::pileStatusChanged,
            this, [this](int, int) { refreshCurrentPage(); });
    m_autoRefreshTimer->start();
}

void AdminMainWindow::onNavChanged(int row)
{
    if (row < 0)
        return;
    m_stack->setCurrentIndex(row);
    m_headerTitle->setText(m_navList->item(row)->data(Qt::UserRole).toString());
    QTimer::singleShot(0, this, &AdminMainWindow::refreshCurrentPage);
}

void AdminMainWindow::refreshCurrentPage()
{
    if (!isVisible() || !m_stack || !m_stack->currentWidget())
        return;
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget())
        return;
    QWidget *page = m_stack->currentWidget();
    QMetaObject::invokeMethod(page, "refreshPage", Qt::DirectConnection);
    // resizeColumnsToContents() 会暂时压缩末列；刷新完成后重新让末列填满卡片。
    for (auto *table : page->findChildren<QTableWidget *>()) {
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setStretchLastSection(true);
    }
}

void AdminMainWindow::onLogoutClicked()
{
    if (QMessageBox::question(this, "提示", "确定要退出登录吗?") == QMessageBox::Yes) {
        ServerSession::instance().reset();
        close();
    }
}

void AdminMainWindow::onOpenWebClicked()
{
    if (m_webUrl.isEmpty()) {
        QMessageBox::warning(this, "提示", "Web 大屏服务未启动, 请检查端口是否被占用");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl(m_webUrl)))
        QMessageBox::warning(this, "提示", "无法自动打开浏览器, 请手动访问: " + m_webUrl);
}

void AdminMainWindow::showConnectionInfo(QTcpServer *server)
{
    auto *panel = new QWidget(this);
    panel->setObjectName("lanConnectionPanel");
    auto *row = new QHBoxLayout(panel);
    auto *label = new QLabel(panel);
    label->setObjectName("lanStatusLabel");
    auto *addresses = new QComboBox(panel);
    addresses->setObjectName("lanAddressCombo");
    addresses->setAccessibleName("服务器 IP 与端口");
    addresses->setMinimumContentsLength(24);
    addresses->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto *copy = new QPushButton("复制地址", panel);
    auto *refresh = new QPushButton("刷新", panel);
    row->addWidget(label);
    row->addWidget(addresses, 1);
    row->addWidget(copy);
    row->addWidget(refresh);
    auto *layout = qobject_cast<QVBoxLayout *>(m_stack->parentWidget()->layout());
    layout->insertWidget(1, panel);
    panel->setToolTip("另一台电脑在客户端登录页填写对应 IP 和端口。多网卡时选择与客户端同一局域网的地址；127.0.0.1 仅供本机使用。");
    const auto update = [server, label, addresses, copy] {
        const QString selected = addresses->currentData().toString();
        addresses->clear();
        if (!server->isListening()) {
            label->setText("客户端服务未启动");
            addresses->addItem("监听失败：" + server->errorString());
            copy->setEnabled(false);
            return;
        }
        const auto port = server->serverPort();
        QStringList seen;
        for (const auto &iface : QNetworkInterface::allInterfaces()) {
            if (!(iface.flags() & QNetworkInterface::IsUp)
                || !(iface.flags() & QNetworkInterface::IsRunning)
                || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
            for (const auto &entry : iface.addressEntries()) {
                const auto ip = entry.ip();
                if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback()
                    || ip.isNull() || ip.isLinkLocal() || seen.contains(ip.toString())) continue;
                seen.append(ip.toString());
                const QString endpoint = QString("%1:%2").arg(ip.toString()).arg(port);
                addresses->addItem(endpoint + "  (" + iface.humanReadableName() + ")", endpoint);
            }
        }
        label->setText(seen.isEmpty() ? "未发现局域网 IP" : "客户端连接地址");
        const QString local = QString("127.0.0.1:%1").arg(port);
        addresses->addItem(local + "  (仅本机)", local);
        const int previous = addresses->findData(selected);
        if (previous >= 0) addresses->setCurrentIndex(previous);
        copy->setEnabled(true);
    };
    connect(copy, &QPushButton::clicked, this, [addresses] {
        QGuiApplication::clipboard()->setText(addresses->currentData().toString());
    });
    connect(refresh, &QPushButton::clicked, this, update);
    auto *timer = new QTimer(panel);
    connect(timer, &QTimer::timeout, this, update);
    timer->start(10000);
    update();
    UiMotion::install(panel);
}
