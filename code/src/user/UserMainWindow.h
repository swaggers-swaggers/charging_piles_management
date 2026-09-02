#ifndef USERMAINWINDOW_H
#define USERMAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

// 用户端主窗口 (模拟手机端交互)
// 四个功能页面对应项目说明书:
//   附近充电站 / 一键导航 / 用户信息 / 电动汽车充电
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
