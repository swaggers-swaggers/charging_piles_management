#include "UiMotion.h"
#include "UserMainWindow.h"

#include "ClientSession.h"
#include "NearbyStationsPage.h"
#include "NavigationPage.h"
#include "UserInfoPage.h"
#include "ChargingPage.h"
#include "OrderHistoryPage.h"
#include "pages/MessagePage.h"
#include "MessageCenter.h"
#include "TcpClient.h"
#include "protocol.h"
#include "IconFactory.h"

#include <QFile>
#include <QDialog>
#include <QTimer>
#include <QGuiApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSize>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

UserMainWindow::UserMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("东软电动汽车充电桩应用管理平台 - 用户端");
    // 尺寸自适应屏幕, 避免在分辨率较小的虚拟机窗口上超出屏幕看不到
    const QSize screen = QGuiApplication::primaryScreen()->availableGeometry().size();
    resize(qMin(1200, qMax(640, screen.width() - 80)),
           qMin(820, qMax(480, screen.height() - 120)));

    initUi();
    UiMotion::install(this);

    statusBar()->showMessage(QString("当前用户: %1 (%2)    |    服务器: %3:%4    |    已连接")
                                 .arg(ClientSession::instance().nickname,
                                      ClientSession::instance().phone,
                                      TcpClient::instance().serverHost(),
                                      QString::number(TcpClient::instance().serverPort())));
}

void UserMainWindow::initUi()
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
    sidebar->setFixedWidth(176);
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
    QLabel *logo = new QLabel("东软充电", logoBox);
    logo->setObjectName("logoLabel");
    logoLayout->addStretch();
    logoLayout->addWidget(logoIcon);
    logoLayout->addWidget(logo);
    logoLayout->addStretch();

    m_navList = new QListWidget(sidebar);
    m_navList->setObjectName("navList");
    const QStringList navNames = {
        "附近充电站", "充电进度", "我的订单", "消息中心", "我的账户",
    };
    const QVector<IconFactory::IconType> navIcons = {
        IconFactory::IconLocation, IconFactory::IconBolt,
        IconFactory::IconChartLine, IconFactory::IconBattery, IconFactory::IconUser,
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
    auto *sideNote = new QLabel("绿色出行\n让每一程更轻松", sidebar);
    sideNote->setObjectName("sideNote");
    sideLayout->addWidget(sideNote);
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

    m_headerUser = new QLabel(QString("%1  |  余额: %2 元")
                                  .arg(ClientSession::instance().nickname)
                                  .arg(ClientSession::instance().balance, 0, 'f', 2),
                              header);
    m_headerUser->setObjectName("headerUser");

    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_headerUser);

    m_stack = new QStackedWidget(rightArea);
    m_stack->setObjectName("contentStack");
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    auto *nearby = new NearbyStationsPage();
    auto *charging = new ChargingPage();
    m_stack->addWidget(nearby);
    auto addScrollablePage = [this](QWidget *page) {
        auto *scroll = new QScrollArea(m_stack);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);
        m_stack->addWidget(scroll);
    };
    addScrollablePage(charging);
    addScrollablePage(new OrderHistoryPage());
    addScrollablePage(new MessagePage());
    addScrollablePage(new UserInfoPage());
    connect(nearby, &NearbyStationsPage::chargeRequested, this, [this, charging](int id) {
        charging->selectStation(id);
        m_navList->setCurrentRow(1);
    });
    connect(nearby, &NearbyStationsPage::navigationRequested, this, [this](int id, double lon, double lat) {
        QDialog dialog(this);
        dialog.setWindowTitle("站点导航");
        dialog.resize(qMin(1000, width()), qMin(720, height()));
        auto *layout = new QVBoxLayout(&dialog);
        auto *back = new QPushButton("返回充电站", &dialog);
        layout->addWidget(back, 0, Qt::AlignLeft);
        auto *navigation = new NavigationPage(&dialog);
        navigation->setDestination(id, lon, lat);
        layout->addWidget(navigation, 1);
        connect(back, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.exec();
    });
    auto *balanceTimer = new QTimer(this);
    connect(balanceTimer, &QTimer::timeout, this, [this] {
        m_headerUser->setText(QString("%1  |  余额: %2 元")
            .arg(ClientSession::instance().nickname)
            .arg(ClientSession::instance().balance, 0, 'f', 2));
    });
    balanceTimer->start(1000);

    // 消息中心未读角标: 导航项文本后追加未读数
    auto updateMsgBadge = [this](int unread) {
        if (m_navList->count() <= 3) return;
        auto *item = m_navList->item(3);
        if (!item) return;
        item->setText(unread > 0 ? QString("消息中心 (%1)").arg(unread)
                                  : QString("消息中心"));
    };
    updateMsgBadge(MessageCenter::instance().unreadCount());
    connect(&MessageCenter::instance(), &MessageCenter::unreadCountChanged,
            this, updateMsgBadge);

    rightLayout->addWidget(header);
    rightLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(rightArea, 1);

    connect(m_navList, &QListWidget::currentRowChanged,
            this, &UserMainWindow::onNavChanged);
    connect(logoutBtn, &QPushButton::clicked,
            this, &UserMainWindow::onLogoutClicked);
    connect(&TcpClient::instance(), &TcpClient::connectionLost, this, [this]() {
        statusBar()->showMessage("服务端连接已断开，请重新操作以重连");
        QMessageBox::warning(this, "连接断开", "服务端连接已断开，请检查服务端后重试。");
    });
}

void UserMainWindow::onNavChanged(int row)
{
    if (row < 0)
        return;
    m_stack->setCurrentIndex(row);
    m_headerTitle->setText(m_navList->item(row)->data(Qt::UserRole).toString());
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
