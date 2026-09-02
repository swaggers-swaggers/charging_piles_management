#include "AdminLoginDialog.h"

#include "DatabaseManager.h"
#include "ServerSession.h"

#include <QCheckBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>

AdminLoginDialog::AdminLoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("loginDialog");
    setWindowTitle("东软电动汽车充电桩应用管理平台 - 服务端");
    setFixedSize(420, 470);

    loadStyleSheet();
    initUi();
    initConnections();

    // 恢复上次登录的管理员账号
    QSettings settings;
    const QString lastName = settings.value("login/lastAdmin").toString();
    m_nameEdit->setText(lastName);
    m_rememberChk->setChecked(!lastName.isEmpty());
}

void AdminLoginDialog::loadStyleSheet()
{
    QFile file(":/qss/login.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void AdminLoginDialog::initUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(32, 30, 32, 16);
    rootLayout->setSpacing(10);

    QLabel *title = new QLabel("⚡ 充电桩管理平台", this);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);

    QLabel *subtitle = new QLabel("服 务 端 · 管 理 员 登 录", this);
    subtitle->setObjectName("subtitleLabel");
    subtitle->setAlignment(Qt::AlignCenter);

    QWidget *card = new QWidget(this);
    card->setObjectName("card");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(26, 28, 26, 26);
    cardLayout->setSpacing(14);

    m_nameEdit = new QLineEdit(card);
    m_nameEdit->setPlaceholderText("请输入管理员账号");

    m_pwdEdit = new QLineEdit(card);
    m_pwdEdit->setPlaceholderText("请输入管理员密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);

    QHBoxLayout *optionRow = new QHBoxLayout();
    m_rememberChk = new QCheckBox("记住账号", card);
    optionRow->addWidget(m_rememberChk);
    optionRow->addStretch();

    m_loginBtn = new QPushButton("登 录", card);
    m_loginBtn->setObjectName("adminLoginBtn");
    m_loginBtn->setCursor(Qt::PointingHandCursor);

    cardLayout->addWidget(m_nameEdit);
    cardLayout->addWidget(m_pwdEdit);
    cardLayout->addLayout(optionRow);
    cardLayout->addWidget(m_loginBtn);

    QLabel *hint = new QLabel("默认账号:admin  密码:123456 (本机数据库校验)", this);
    hint->setObjectName("hintLabel");
    hint->setAlignment(Qt::AlignCenter);

    QPushButton *exitBtn = new QPushButton("退出程序", this);
    exitBtn->setObjectName("exitBtn");
    exitBtn->setCursor(Qt::PointingHandCursor);

    rootLayout->addWidget(title);
    rootLayout->addWidget(subtitle);
    rootLayout->addSpacing(10);
    rootLayout->addWidget(card, 1);
    rootLayout->addSpacing(6);
    rootLayout->addWidget(hint);
    rootLayout->addWidget(exitBtn, 0, Qt::AlignRight);

    connect(exitBtn, &QPushButton::clicked, this, &QWidget::close);
}

void AdminLoginDialog::initConnections()
{
    connect(m_loginBtn, &QPushButton::clicked,
            this, &AdminLoginDialog::onLoginClicked);
    connect(m_nameEdit, &QLineEdit::returnPressed,
            m_loginBtn, &QPushButton::click);
    connect(m_pwdEdit, &QLineEdit::returnPressed,
            m_loginBtn, &QPushButton::click);
}

void AdminLoginDialog::onLoginClicked()
{
    const QString name = m_nameEdit->text().trimmed();
    const QString pwd = m_pwdEdit->text();

    if (name.isEmpty() || pwd.isEmpty()) {
        showWarning("请输入账号和密码!");
        (name.isEmpty() ? m_nameEdit : m_pwdEdit)->setFocus();
        return;
    }

    int adminId = -1;
    QString errMsg;
    if (!DatabaseManager::instance().verifyAdmin(name, pwd, &adminId, &errMsg)) {
        showWarning(errMsg);
        m_pwdEdit->clear();
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
        return;
    }

    QSettings settings;
    if (m_rememberChk->isChecked())
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
