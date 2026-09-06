#ifndef USERMAINWINDOW_H
#define USERMAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

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

private:
    void initUi();

    QListWidget *m_navList;
    QStackedWidget *m_stack;
    QLabel *m_headerTitle;
    QLabel *m_headerUser;
};

#endif // USERMAINWINDOW_H
