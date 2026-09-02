#include "LoginDialog.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QCheckBox>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("loginDialog");
    setWindowTitle("东软电动汽车充电桩应用管理平台");
    setFixedSize(440, 540);

    loadStyleSheet();
    initUi();
    initConnections();

    // 恢复上次登录的手机号
    QSettings settings;
    const QString lastPhone = settings.value("login/lastPhone").toString();
    m_phoneEdit->setText(lastPhone);
    m_rememberChk->setChecked(!lastPhone.isEmpty());
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

    QLabel *title = new QLabel("⚡ 东软充电", this);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);

    QLabel *subtitle = new QLabel("电 动 汽 车 充 电 服 务", this);
    subtitle->setObjectName("subtitleLabel");
    subtitle->setAlignment(Qt::AlignCenter);

    QWidget *card = new QWidget(this);
    card->setObjectName("card");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(26, 28, 26, 26);
    cardLayout->setSpacing(14);

    m_phoneEdit = new QLineEdit(card);
    m_phoneEdit->setPlaceholderText("请输入11位手机号");
    m_phoneEdit->setMaxLength(11);
    // 限制只能输入 1 开头的数字(允许输入过程中的中间状态)
    m_phoneEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("^1\\d{0,10}$"), m_phoneEdit));

    QHBoxLayout *optionRow = new QHBoxLayout();
    m_rememberChk = new QCheckBox("记住手机号", card);
    optionRow->addWidget(m_rememberChk);
    optionRow->addStretch();

    m_loginBtn = new QPushButton("登录 / 注册", card);
    m_loginBtn->setObjectName("userLoginBtn");
    m_loginBtn->setCursor(Qt::PointingHandCursor);

    QLabel *tip = new QLabel("手机号首次登录将自动注册新用户", card);
    tip->setObjectName("tipLabel");
    tip->setAlignment(Qt::AlignCenter);

    cardLayout->addWidget(m_phoneEdit);
    cardLayout->addLayout(optionRow);
    cardLayout->addWidget(m_loginBtn);
    cardLayout->addWidget(tip);
    cardLayout->addStretch();

    QLabel *hint = new QLabel(QString("需要先启动服务端 ChargingServer (%1:%2)")
                                  .arg(Protocol::serverHost())
                                  .arg(Protocol::serverPort()),
                              this);
    hint->setObjectName("hintLabel");
    hint->setAlignment(Qt::AlignCenter);

    QPushButton *exitBtn = new QPushButton("退出程序", this);
    exitBtn->setObjectName("exitBtn");
    exitBtn->setCursor(Qt::PointingHandCursor);

    rootLayout->addWidget(title);
    rootLayout->addWidget(subtitle);
    rootLayout->addSpacing(14);
    rootLayout->addWidget(card, 1);
    rootLayout->addSpacing(6);
    rootLayout->addWidget(hint);
    rootLayout->addWidget(exitBtn, 0, Qt::AlignRight);

    connect(exitBtn, &QPushButton::clicked, this, &QWidget::close);
}

void LoginDialog::initConnections()
{
    connect(m_loginBtn, &QPushButton::clicked,
            this, &LoginDialog::onLoginClicked);
    connect(m_phoneEdit, &QLineEdit::returnPressed,
            m_loginBtn, &QPushButton::click);
}

void LoginDialog::onLoginClicked()
{
    const QString phone = m_phoneEdit->text().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch()) {
        showWarning("请输入正确的11位手机号!");
        m_phoneEdit->setFocus();
        return;
    }

    QString connErr;
    if (!TcpClient::instance().ensureConnected(3000, &connErr)) {
        showWarning("无法连接服务器:\n" + connErr +
                    "\n\n请先启动服务端 ChargingServer 再登录");
        return;
    }

    bool ok = false;
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUserLogin, QJsonObject{{"phone", phone}}, 5000, &ok);
    if (!ok) {
        showWarning(reply.value("error").toString("登录失败!"));
        m_phoneEdit->clear();
        m_phoneEdit->setFocus();
        return;
    }

    QSettings settings;
    if (m_rememberChk->isChecked())
        settings.setValue("login/lastPhone", phone);
    else
        settings.remove("login/lastPhone");

    ClientSession &s = ClientSession::instance();
    s.userId = reply.value("userId").toInt();
    s.phone = reply.value("phone").toString();
    s.nickname = reply.value("nickname").toString();
    s.balance = reply.value("balance").toDouble();

    const bool isNewUser = reply.value("isNew").toBool();
    QMessageBox::information(this, "提示",
                             QString(isNewUser ? "注册成功, 欢迎 %1 !"
                                               : "登录成功, 欢迎 %1 !").arg(s.nickname));
    accept();
}

void LoginDialog::showWarning(const QString &text)
{
    QMessageBox::warning(this, "警告", text, QMessageBox::Yes);
}
