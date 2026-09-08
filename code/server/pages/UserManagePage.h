#ifndef USERMANAGEPAGE_H
#define USERMANAGEPAGE_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;

// 用户管理页: 用户列表 + 手机号/昵称模糊搜索 + 冻结/解冻
class UserManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit UserManagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onFreezeClicked();
    void onSelectionChanged();

private:
    QLineEdit *m_searchEdit;
    QTableWidget *m_table;
    QPushButton *m_freezeBtn;
    int m_selectedUserId = -1;
    int m_selectedStatus = -1;
    QTimer *m_autoRefresh = nullptr;   // 每 3 秒自动刷新, 无需手动点刷新按钮
};

#endif // USERMANAGEPAGE_H
