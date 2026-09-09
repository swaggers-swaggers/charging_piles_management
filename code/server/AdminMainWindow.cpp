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
#include "HoverSidebar.h"
#include "AsymmetricGradientCanvas.h"

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
    QWidget *central = new AsymmetricGradientCanvas(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 0, 10);
    rootLayout->setSpacing(10);
    setCentralWidget(central);

    // ---------- 左侧导航 ----------
    auto *sidebar = new HoverSidebar(210, central);
    sidebar->setAccessibleName(QStringLiteral("悬停展开导航栏"));
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
        "首页", "电桩状态", "充电站与电桩管理", "订单管理", "用户管理",
    };
    const QVector<IconFactory::IconType> navIcons = {
        IconFactory::IconHome, IconFactory::IconBattery, IconFactory::IconBuilding,
        IconFactory::IconBolt, IconFactory::IconUsers,
    };
    for (int i = 0; i < navNames.size(); ++i) {
        auto *item = new QListWidgetItem(navNames[i]);
        item->setIcon(IconFactory::navigationIcon(navIcons[i]));
        item->setData(Qt::UserRole, navNames[i]);   // 纯文本标题(不含图标)
        item->setData(HoverSidebar::FullTextRole, navNames[i]);
        m_navList->addItem(item);
    }
    m_navList->setIconSize(QSize(20, 20));
    m_navList->setCurrentRow(0);
    m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QPushButton *logoutBtn = new QPushButton("退出登录", sidebar);
    logoutBtn->setObjectName("logoutBtn");
    logoutBtn->setIcon(IconFactory::navigationIcon(IconFactory::IconLogout));
    logoutBtn->setIconSize(QSize(20, 20));
    logoutBtn->setCursor(Qt::PointingHandCursor);

    sideLayout->addWidget(logoBox);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(m_navList, 1);
    sideLayout->addWidget(logoutBtn);
    sidebar->setNavigationList(m_navList);
    sidebar->addExpandedOnly(logo);
    sidebar->setActionButton(logoutBtn, QStringLiteral("退出登录"));

    // ---------- 右侧页面栈：取消重复顶栏，管理员与连接状态统一进入首页 ----------
    QWidget *rightArea = new QWidget(central);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_stack = new QStackedWidget(rightArea);
    m_stack->setObjectName("contentStack");
    m_homePage = new SalesPage();
    m_stack->addWidget(m_homePage);
    m_stack->addWidget(new PileStatusPage());
    m_stack->addWidget(new StationManagePage());
    m_stack->addWidget(new OrderManagePage());
    m_stack->addWidget(new UserManagePage());

    rightLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(rightArea, 1);

    connect(m_navList, &QListWidget::currentRowChanged,
            this, &AdminMainWindow::onNavChanged);
    connect(logoutBtn, &QPushButton::clicked,
            this, &AdminMainWindow::onLogoutClicked);
    connect(m_homePage, &SalesPage::pageRequested,
            this, [this](int pageIndex) { m_navList->setCurrentRow(pageIndex); });
    connect(m_homePage, &SalesPage::openWebRequested,
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
    if (m_homePage)
        m_homePage->showConnectionInfo(server, m_serverInfo, m_webUrl);
}
