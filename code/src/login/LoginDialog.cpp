#include "LoginDialog.h"

#include "DatabaseManager.h"
#include "Session.h"

#include <QCheckBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("loginDialog");
    setWindowTitle("东软电动汽车充电桩应用管理平台");
    setFixedSize(460, 570);

    loadStyleSheet();
    initUi();
    initConnections();

    // 恢复上次登录的管理员账号
    QSettings settings;
    const QString lastName = settings.value("login/lastAdmin").toString();
    m_adminNameEdit->setText(lastName);
    m_rememberChk->setChecked(!lastName.isEmpty());
}

void LoginDialog::loadStyleSheet()
{
    QFile file(":/qss/login.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void LoginDialog::initUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(36, 34, 36, 18);
    rootLayout->setSpacing(10);

    QLabel *title = new QLabel("⚡ 东软电动汽车充电桩", this);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);

    QLabel *subtitle = new QLabel("应 用 管 理 平 台", this);
    subtitle->setObjectName("subtitleLabel");
    subtitle->setAlignment(Qt::AlignCenter);

    m_tabWidget = new QTabWidget(this);

    // ---------- 用户登录页 ----------
    QWidget *userTab = new QWidget(m_tabWidget);
    QVBoxLayout *userLayout = new QVBoxLayout(userTab);
    userLayout->setContentsMargins(26, 28, 26, 22);
    userLayout->setSpacing(14);

    m_phoneEdit = new QLineEdit(userTab);
    m_phoneEdit->setPlaceholderText("请输入11位手机号");
    m_phoneEdit->setMaxLength(11);
    // 限制只能输入 1 开头的数字(允许输入过程中的中间状态)
    m_phoneEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("^1\\d{0,10}$"), m_phoneEdit));

    m_userLoginBtn = new QPushButton("登录 / 注册", userTab);
    m_userLoginBtn->setObjectName("userLoginBtn");
    m_userLoginBtn->setCursor(Qt::PointingHandCursor);

    QLabel *userTip = new QLabel("手机号首次登录将自动注册新用户", userTab);
    userTip->setObjectName("tipLabel");
    userTip->setAlignment(Qt::AlignCenter);

    userLayout->addWidget(m_phoneEdit);
    userLayout->addWidget(m_userLoginBtn);
    userLayout->addWidget(userTip);
    userLayout->addStretch();
    m_tabWidget->addTab(userTab, "用户登录");

    // ---------- 管理员登录页 ----------
    QWidget *adminTab = new QWidget(m_tabWidget);
    QVBoxLayout *adminLayout = new QVBoxLayout(adminTab);
    adminLayout->setContentsMargins(26, 28, 26, 22);
    adminLayout->setSpacing(14);

    m_adminNameEdit = new QLineEdit(adminTab);
    m_adminNameEdit->setPlaceholderText("请输入管理员账号");

    m_adminPwdEdit = new QLineEdit(adminTab);
    m_adminPwdEdit->setPlaceholderText("请输入管理员密码");
    m_adminPwdEdit->setEchoMode(QLineEdit::Password);

    QHBoxLayout *optionRow = new QHBoxLayout();
    m_rememberChk = new QCheckBox("记住账号", adminTab);
    optionRow->addWidget(m_rememberChk);
    optionRow->addStretch();

    m_adminLoginBtn = new QPushButton("登 录", adminTab);
    m_adminLoginBtn->setObjectName("adminLoginBtn");
    m_adminLoginBtn->setCursor(Qt::PointingHandCursor);

    adminLayout->addWidget(m_adminNameEdit);
    adminLayout->addWidget(m_adminPwdEdit);
    adminLayout->addLayout(optionRow);
    adminLayout->addWidget(m_adminLoginBtn);
    adminLayout->addStretch();
    m_tabWidget->addTab(adminTab, "管理员登录");

    QLabel *hint = new QLabel("默认管理员账号:admin  密码:123456", this);
    hint->setObjectName("hintLabel");
    hint->setAlignment(Qt::AlignCenter);

    QPushButton *exitBtn = new QPushButton("退出程序", this);
    exitBtn->setObjectName("exitBtn");
    exitBtn->setCursor(Qt::PointingHandCursor);

    rootLayout->addWidget(title);
    rootLayout->addWidget(subtitle);
    rootLayout->addSpacing(14);
    rootLayout->addWidget(m_tabWidget, 1);
    rootLayout->addSpacing(6);
    rootLayout->addWidget(hint);
    rootLayout->addWidget(exitBtn, 0, Qt::AlignRight);

    connect(exitBtn, &QPushButton::clicked, this, &QWidget::close);
}

void LoginDialog::initConnections()
{
    connect(m_userLoginBtn, &QPushButton::clicked,
            this, &LoginDialog::onUserLoginClicked);
    connect(m_phoneEdit, &QLineEdit::returnPressed,
            m_userLoginBtn, &QPushButton::click);

    connect(m_adminLoginBtn, &QPushButton::clicked,
            this, &LoginDialog::onAdminLoginClicked);
    connect(m_adminNameEdit, &QLineEdit::returnPressed,
            m_adminLoginBtn, &QPushButton::click);
    connect(m_adminPwdEdit, &QLineEdit::returnPressed,
            m_adminLoginBtn, &QPushButton::click);
}

void LoginDialog::onUserLoginClicked()
{
    const QString phone = m_phoneEdit->text().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch()) {
        showWarning("请输入正确的11位手机号!");
        m_phoneEdit->setFocus();
        return;
    }

    UserInfo info;
    bool isNewUser = false;
    QString errMsg;
    if (!DatabaseManager::instance().loginOrRegisterUser(phone, &info, &isNewUser, &errMsg)) {
        showWarning(errMsg.isEmpty() ? "登录失败!" : errMsg);
        m_phoneEdit->clear();
        m_phoneEdit->setFocus();
        return;
    }

    Session &s = Session::instance();
    s.isAdmin = false;
    s.userId = info.id;
    s.phone = info.phone;
    s.nickname = info.nickname;
    s.balance = info.balance;

    QMessageBox::information(this, "提示",
                             QString(isNewUser ? "注册成功, 欢迎 %1 !"
                                               : "登录成功, 欢迎 %1 !").arg(info.nickname));
    accept();
}

void LoginDialog::onAdminLoginClicked()
{
    const QString name = m_adminNameEdit->text().trimmed();
    const QString pwd = m_adminPwdEdit->text();

    if (name.isEmpty() || pwd.isEmpty()) {
        showWarning("请输入账号和密码!");
        (name.isEmpty() ? m_adminNameEdit : m_adminPwdEdit)->setFocus();
        return;
    }

    int adminId = -1;
    QString errMsg;
    if (!DatabaseManager::instance().verifyAdmin(name, pwd, &adminId, &errMsg)) {
        showWarning(errMsg);
        // 登录失败: 清空密码并重新聚焦(账号保留, 配合"记住账号")
        m_adminPwdEdit->clear();
        m_adminNameEdit->setFocus();
        m_adminNameEdit->selectAll();
        return;
    }

    QSettings settings;
    if (m_rememberChk->isChecked())
        settings.setValue("login/lastAdmin", name);
    else
        settings.remove("login/lastAdmin");

    Session &s = Session::instance();
    s.isAdmin = true;
    s.adminId = adminId;
    s.adminName = name;

    QMessageBox::information(this, "提示", "登录成功!");
    accept();
}

void LoginDialog::showWarning(const QString &text)
{
    QMessageBox::warning(this, "警告", text, QMessageBox::Yes);
}
