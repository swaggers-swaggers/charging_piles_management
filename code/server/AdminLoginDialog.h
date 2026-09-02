#ifndef ADMINLOGINDIALOG_H
#define ADMINLOGINDIALOG_H

#include <QDialog>

namespace Ui {
class AdminLoginDialog;
}

// 服务端管理员登录对话框(界面由 Qt Designer 的 AdminLoginDialog.ui 驱动)
// 本机数据库校验, 默认 admin / 123456
class AdminLoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdminLoginDialog(QWidget *parent = nullptr);
    ~AdminLoginDialog() override;

private slots:
    void onLoginClicked();

private:
    void initConnections();
    void loadStyleSheet();
    void showWarning(const QString &text);

    Ui::AdminLoginDialog *ui;
};

#endif // ADMINLOGINDIALOG_H
