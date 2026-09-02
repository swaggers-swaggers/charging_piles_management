#include "AdminLoginDialog.h"
#include "ui_AdminLoginDialog.h"

#include "DatabaseManager.h"
#include "ServerSession.h"

#include <QFile>
#include <QMessageBox>
#include <QSettings>

AdminLoginDialog::AdminLoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AdminLoginDialog)
{
    ui->setupUi(this);
    setFixedSize(420, 470);

    loadStyleSheet();

    // 恢复上次登录的管理员账号
    QSettings settings;
    const QString lastName = settings.value("login/lastAdmin").toString();
    ui->nameEdit->setText(lastName);
    ui->rememberChk->setChecked(!lastName.isEmpty());

    initConnections();
}

AdminLoginDialog::~AdminLoginDialog()
{
    delete ui;
}

void AdminLoginDialog::loadStyleSheet()
{
    QFile file(":/qss/login.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void AdminLoginDialog::initConnections()
{
    connect(ui->loginBtn, &QPushButton::clicked, this, &AdminLoginDialog::onLoginClicked);
    connect(ui->nameEdit, &QLineEdit::returnPressed, ui->loginBtn, &QPushButton::click);
    connect(ui->pwdEdit, &QLineEdit::returnPressed, ui->loginBtn, &QPushButton::click);
    connect(ui->exitBtn, &QPushButton::clicked, this, &QWidget::close);
}

void AdminLoginDialog::onLoginClicked()
{
    const QString name = ui->nameEdit->text().trimmed();
    const QString pwd = ui->pwdEdit->text();

    if (name.isEmpty() || pwd.isEmpty()) {
        showWarning("请输入账号和密码!");
        (name.isEmpty() ? ui->nameEdit : ui->pwdEdit)->setFocus();
        return;
    }

    int adminId = -1;
    QString errMsg;
    if (!DatabaseManager::instance().verifyAdmin(name, pwd, &adminId, &errMsg)) {
        showWarning(errMsg);
        ui->pwdEdit->clear();
        ui->nameEdit->setFocus();
        ui->nameEdit->selectAll();
        return;
    }

    QSettings settings;
    if (ui->rememberChk->isChecked())
        settings.setValue("login/lastAdmin", name);
    else
        settings.remove("login/lastAdmin");

    ServerSession &s = ServerSession::instance();
    s.adminId = adminId;
    s.adminName = name;

    QMessageBox::information(this, "提示", "登录成功!");
    accept();
}

void AdminLoginDialog::showWarning(const QString &text)
{
    QMessageBox::warning(this, "警告", text, QMessageBox::Yes);
}
