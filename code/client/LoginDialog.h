#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;

// 用户客户端登录对话框
// 手机号免密登录(项目说明书): 11位手机号经 Socket 发给服务端校验,
// 已注册直接登录(冻结账号拦截), 未注册自动创建新用户(昵称"用户"+手机号后4位)
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();

private:
    void initUi();
    void initConnections();
    void loadStyleSheet();
    void showWarning(const QString &text);

    QLineEdit *m_phoneEdit;
    QCheckBox *m_rememberChk;
    QPushButton *m_loginBtn;
};

#endif // LOGINDIALOG_H
