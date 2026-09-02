#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTabWidget;

// 登录对话框
// 两种身份:
//   用户登录   - 输入 11 位手机号, 已存在则直接登录(冻结账号拦截), 不存在自动注册
//   管理员登录 - 账号密码校验 admin 表, 默认 admin / 123456
// 登录成功后 accept() 关闭, 身份信息写入 Session, main 根据身份打开对应主窗口
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

private slots:
    void onUserLoginClicked();
    void onAdminLoginClicked();

private:
    void initUi();
    void initConnections();
    void loadStyleSheet();
    void showWarning(const QString &text);

    QTabWidget *m_tabWidget;

    // 用户登录页
    QLineEdit *m_phoneEdit;
    QPushButton *m_userLoginBtn;

    // 管理员登录页
    QLineEdit *m_adminNameEdit;
    QLineEdit *m_adminPwdEdit;
    QCheckBox *m_rememberChk;
    QPushButton *m_adminLoginBtn;
};

#endif // LOGINDIALOG_H
