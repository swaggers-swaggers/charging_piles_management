#ifndef ADMINMAINWINDOW_H
#define ADMINMAINWINDOW_H

#include <QMainWindow>

class QTcpServer;
class QListWidget;
class QStackedWidget;
class QTimer;
class SalesPage;

// 服务端管理后台主窗口
// 左侧导航 + 右侧页面栈, 五个功能页面:
//   首页 / 电桩状态 / 充电站与电桩管理 / 订单管理 / 用户管理
class AdminMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // serverInfo: 服务端口监听状态等展示在状态栏的信息
    // webUrl: Web 大屏地址(如 http://localhost:8080), 为空表示 HTTP 服务未启动
    explicit AdminMainWindow(const QString &serverInfo = QString(),
                             const QString &webUrl = QString(),
                             QWidget *parent = nullptr);

    void showConnectionInfo(QTcpServer *server);

private slots:
    void onNavChanged(int row);
    void onLogoutClicked();
    void onOpenWebClicked();
    void refreshCurrentPage();

private:
    void initUi();

    QString m_serverInfo;
    QString m_webUrl;
    QListWidget *m_navList;
    QStackedWidget *m_stack;
    SalesPage *m_homePage = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;
};

#endif // ADMINMAINWINDOW_H
