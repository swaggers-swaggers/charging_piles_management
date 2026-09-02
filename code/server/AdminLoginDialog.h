#ifndef ADMINLOGINDIALOG_H
#define ADMINLOGINDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;

// 服务端管理员登录对话框(本机数据库校验, 默认 admin / 123456)
class AdminLoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdminLoginDialog(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();

private:
    void initUi();
    void initConnections();
    void loadStyleSheet();
    void showWarning(const QString &text);

    QLineEdit *m_nameEdit;
    QLineEdit *m_pwdEdit;
    QCheckBox *m_rememberChk;
    QPushButton *m_loginBtn;
};

#endif // ADMINLOGINDIALOG_H
