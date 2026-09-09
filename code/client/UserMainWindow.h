#ifndef USERMAINWINDOW_H
#define USERMAINWINDOW_H

#include <QMainWindow>

class QListWidget;
class QStackedWidget;
class QTimer;

// 用户客户端主窗口 (模拟手机端交互)
// 首页串联查站、导航与选桩；侧栏保留充电进度、订单、消息和账户。
class UserMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit UserMainWindow(QWidget *parent = nullptr);

private slots:
    void onNavChanged(int row);
    void onLogoutClicked();
    void refreshCurrentPage();

private:
    void initUi();

    QListWidget *m_navList;
    QStackedWidget *m_stack;
    QTimer *m_autoRefreshTimer = nullptr;
};

#endif // USERMAINWINDOW_H
