#ifndef ADMINMAINWINDOW_H
#define ADMINMAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

// 管理端主窗口
// 左侧导航 + 右侧页面栈, 五个功能页面对应项目说明书:
//   销售业绩 / 电桩状态 / 充电桩管理 / 充电站管理 / 用户管理
class AdminMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminMainWindow(QWidget *parent = nullptr);

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

#endif // ADMINMAINWINDOW_H
