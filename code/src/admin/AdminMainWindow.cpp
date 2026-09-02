#include "AdminMainWindow.h"

#include "DatabaseManager.h"
#include "Session.h"
#include "SalesPage.h"
#include "PileStatusPage.h"
#include "PileManagePage.h"
#include "StationManagePage.h"
#include "UserManagePage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

AdminMainWindow::AdminMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("东软电动汽车充电桩应用管理平台 - 管理端");
    resize(1100, 700);

    initUi();

    statusBar()->showMessage(QString("管理员: %1    |    数据库: %2    |    连接正常")
                                 .arg(Session::instance().adminName,
                                      DatabaseManager::instance().databasePath()));
}

void AdminMainWindow::initUi()
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

    QLabel *logo = new QLabel("⚡ 充电桩管理平台", sidebar);
    logo->setObjectName("logoLabel");
    logo->setAlignment(Qt::AlignCenter);

    m_navList = new QListWidget(sidebar);
    m_navList->setObjectName("navList");
    const QStringList navItems = {
        "销售业绩", "电桩状态", "充电桩管理", "充电站管理", "用户管理",
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
    m_headerUser = new QLabel("管理员: " + Session::instance().adminName, header);
    m_headerUser->setObjectName("headerUser");

    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_headerUser);

    m_stack = new QStackedWidget(rightArea);
    m_stack->addWidget(new SalesPage());
    m_stack->addWidget(new PileStatusPage());
    m_stack->addWidget(new PileManagePage());
    m_stack->addWidget(new StationManagePage());
    m_stack->addWidget(new UserManagePage());

    rightLayout->addWidget(header);
    rightLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(rightArea, 1);

    connect(m_navList, &QListWidget::currentRowChanged,
            this, &AdminMainWindow::onNavChanged);
    connect(logoutBtn, &QPushButton::clicked,
            this, &AdminMainWindow::onLogoutClicked);
}

void AdminMainWindow::onNavChanged(int row)
{
    if (row < 0)
        return;
    m_stack->setCurrentIndex(row);
    m_headerTitle->setText(m_navList->item(row)->text());
}

void AdminMainWindow::onLogoutClicked()
{
    if (QMessageBox::question(this, "提示", "确定要退出登录吗?") == QMessageBox::Yes) {
        Session::instance().reset();
        close();
    }
}
