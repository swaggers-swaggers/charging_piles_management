#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

// 用户客户端登录对话框(界面由 Qt Designer 的 LoginDialog.ui 驱动)
// 手机号免密登录(项目说明书): 11位手机号经 Socket 发给服务端校验,
// 已注册直接登录(冻结账号拦截), 未注册自动创建新用户(昵称"用户"+手机号后4位)
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog() override;

private slots:
    void onLoginClicked();

private:
    void initConnections();
    void loadStyleSheet();
    void showWarning(const QString &text);

    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
