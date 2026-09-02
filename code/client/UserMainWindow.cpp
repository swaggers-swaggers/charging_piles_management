#include "UserMainWindow.h"

#include "ClientSession.h"
#include "NearbyStationsPage.h"
#include "NavigationPage.h"
#include "UserInfoPage.h"
#include "ChargingPage.h"
#include "protocol.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

UserMainWindow::UserMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("东软电动汽车充电桩应用管理平台 - 用户端");
    // 尺寸自适应屏幕, 避免在分辨率较小的虚拟机窗口上超出屏幕看不到
    const QSize screen = QGuiApplication::primaryScreen()->availableGeometry().size();
    resize(qMin(1000, qMax(640, screen.width() - 80)),
           qMin(680, qMax(480, screen.height() - 120)));

    initUi();

    statusBar()->showMessage(QString("当前用户: %1 (%2)    |    服务器: %3:%4    |    已连接")
                                 .arg(ClientSession::instance().nickname,
                                      ClientSession::instance().phone,
                                      Protocol::serverHost(),
                                      QString::number(Protocol::serverPort())));
}

void UserMainWindow::initUi()
{
    QWidget *central = new QWidget(this);
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

    QLabel *logo = new QLabel("⚡ 东软充电", sidebar);
    logo->setObjectName("logoLabel");
    logo->setAlignment(Qt::AlignCenter);

    m_navList = new QListWidget(sidebar);
    m_navList->setObjectName("navList");
    const QStringList navItems = {
        "附近充电站", "一键导航", "用户信息", "电动汽车充电",
    };
    m_navList->addItems(navItems);
    m_navList->setCurrentRow(0);
    m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QPushButton *logoutBtn = new QPushButton("退出登录", sidebar);
    logoutBtn->setObjectName("logoutBtn");
    logoutBtn->setCursor(Qt::PointingHandCursor);

    sideLayout->addWidget(logo);
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

    m_headerTitle = new QLabel(navItems.first(), header);
    m_headerTitle->setObjectName("headerTitle");

    m_headerUser = new QLabel(QString("%1  |  余额: %2 元")
                                  .arg(ClientSession::instance().nickname)
                                  .arg(ClientSession::instance().balance, 0, 'f', 2),
                              header);
    m_headerUser->setObjectName("headerUser");

    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_headerUser);

    m_stack = new QStackedWidget(rightArea);
    m_stack->addWidget(new NearbyStationsPage());
    m_stack->addWidget(new NavigationPage());
    m_stack->addWidget(new UserInfoPage());
    m_stack->addWidget(new ChargingPage());

    rightLayout->addWidget(header);
    rightLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(rightArea, 1);

    connect(m_navList, &QListWidget::currentRowChanged,
            this, &UserMainWindow::onNavChanged);
    connect(logoutBtn, &QPushButton::clicked,
            this, &UserMainWindow::onLogoutClicked);
}

void UserMainWindow::onNavChanged(int row)
{
    if (row < 0)
        return;
    m_stack->setCurrentIndex(row);
    m_headerTitle->setText(m_navList->item(row)->text());
    // 每次切换页面刷新头部(余额可能被充值/结算改变)
    m_headerUser->setText(QString("%1  |  余额: %2 元")
                              .arg(ClientSession::instance().nickname)
                              .arg(ClientSession::instance().balance, 0, 'f', 2));
}

void UserMainWindow::onLogoutClicked()
{
    if (QMessageBox::question(this, "提示", "确定要退出登录吗?") == QMessageBox::Yes) {
        ClientSession::instance().reset();
        close();
    }
}
