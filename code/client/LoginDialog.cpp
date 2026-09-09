#include "UiMotion.h"
#include "LoginDialog.h"
#include "ui_LoginDialog.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QFile>
#include <QLabel>
#include <QGuiApplication>
#include <QScreen>
#include "IconFactory.h"
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QScopedValueRollback>

namespace {
// 记住手机号: 用异或混淆后存 hex, 避免明文直接出现在配置文件/注册表中
// (客户端本地的轻量防护; 数据库侧手机号由服务端哈希+脱敏存储, 见 DatabaseManager)
const QByteArray kPhoneKey = "Neusoft-Charging-Key!";

QByteArray xorCrypt(const QByteArray &data)
{
    QByteArray out = data;
    for (int i = 0; i < out.size(); ++i)
        out[i] = out[i] ^ kPhoneKey[i % kPhoneKey.size()];
    return out;
}

QString encPhone(const QString &phone)
{
    return QString::fromLatin1(xorCrypt(phone.toUtf8()).toHex());
}

QString decPhone(const QString &cipher)
{
    const QByteArray raw = QByteArray::fromHex(cipher.toLatin1());
    return QString::fromUtf8(xorCrypt(raw));
}

// 读取上次记住的手机号: 兼容旧版明文(11位数字), 新版为 hex 密文(22字符)
QString loadRememberedPhone()
{
    QSettings settings;
    const QString v = settings.value("login/lastPhone").toString();
    if (v.isEmpty())
        return QString();
    if (v.length() == 11 && v.startsWith('1'))
        return v;                       // 旧版明文, 下次保存时自动升级为密文
    if (v.length() == 22)
        return decPhone(v);
    return QString();
}
} // namespace

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    UiMotion::install(this);
    setMinimumSize(440, 580);
    const QSize screen = QGuiApplication::primaryScreen()->availableGeometry().size();
    resize(qMin(520, screen.width() - 40), qMin(740, screen.height() - 60));
    ui->card->setAttribute(Qt::WA_StyledBackground, true);
    ui->titleLabel->setWordWrap(true);
    ui->subtitleLabel->setWordWrap(true);
    ui->hintLabel->setWordWrap(true);
    ui->loginBtn->setDefault(true);
    ui->exitBtn->setAutoDefault(false);
    auto *mark = new QLabel(this);
    mark->setObjectName("loginMark");
    mark->setPixmap(IconFactory::icon(IconFactory::IconBolt, QColor("#237653")).pixmap(32, 32));
    ui->rootLayout->insertWidget(0, mark, 0, Qt::AlignHCenter);

    loadStyleSheet();

    // 手机号输入校验: 只允许 1 开头的数字(允许输入过程中的中间状态)
    ui->phoneEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("^1\\d{0,10}$"), ui->phoneEdit));

    ui->hintLabel->setText("找站 · 导航 · 充电，一站完成");
    ui->hintLabel->setText("同一局域网：填写服务端显示的 IP；同机测试可填 127.0.0.1");
    ui->connectBtn->setAutoDefault(false);
    QSettings endpointSettings;
    ui->serverHostEdit->setText(endpointSettings.value("network/host", Protocol::serverHost()).toString());
    const int savedPort = endpointSettings.value("network/port", Protocol::serverPort()).toInt();
    ui->serverPortSpin->setValue(savedPort >= 1 && savedPort <= 65535 ? savedPort : 9527);
    ui->phoneEdit->setAccessibleName("手机号");
    ui->phoneEdit->setClearButtonEnabled(true);

    // 恢复上次登录的手机号(密文存储, 不落明文)
    const QString lastPhone = loadRememberedPhone();
    ui->phoneEdit->setText(lastPhone);
    ui->rememberChk->setChecked(!lastPhone.isEmpty());

    initConnections();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::loadStyleSheet()
{
    QFile file(":/qss/login.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void LoginDialog::initConnections()
{
    connect(ui->connectBtn, &QPushButton::clicked, this, [this] { connectServer(); });
    const auto changed = [this] {
        ui->connectionStatus->setText("连接配置已修改，请重新连接或直接登录。");
    };
    connect(ui->serverHostEdit, &QLineEdit::textChanged, this, changed);
    connect(ui->serverPortSpin, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(&TcpClient::instance(), &TcpClient::connectionLost, this, [this] {
        ui->connectionStatus->setText("连接已断开，请重新连接服务器。");
    });
    connect(ui->loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->phoneEdit, &QLineEdit::returnPressed, ui->loginBtn, &QPushButton::click);
    connect(ui->exitBtn, &QPushButton::clicked, this, &QWidget::close);
}

void LoginDialog::setBusy(bool busy)
{
    ui->serverHostEdit->setEnabled(!busy);
    ui->serverPortSpin->setEnabled(!busy);
    ui->connectBtn->setEnabled(!busy);
    ui->loginBtn->setEnabled(!busy);
    ui->phoneEdit->setEnabled(!busy);
    ui->exitBtn->setEnabled(!busy);
}

bool LoginDialog::connectServer()
{
    if (m_busy) return false;
    QScopedValueRollback<bool> guard(m_busy, true);
    auto &client = TcpClient::instance();
    if (!client.setEndpoint(ui->serverHostEdit->text(), ui->serverPortSpin->value())) {
        ui->connectionStatus->setText("请输入有效的 IPv4 地址，例如 192.168.1.100，端口范围为 1–65535。");
        ui->serverHostEdit->setFocus();
        return false;
    }
    setBusy(true);
    ui->connectionStatus->setText("正在连接服务器，请稍候…");
    QString error;
    bool ok = client.ensureConnected(3000, &error);
    if (ok) {
        const auto reply = client.request(Protocol::ReqHeartbeat, {}, 3000, &ok);
        if (!ok) error = reply.value("error").toString("服务端未响应");
    }
    setBusy(false);
    if (ok) {
        QSettings settings;
        settings.setValue("network/host", client.serverHost());
        settings.setValue("network/port", client.serverPort());
        ui->connectionStatus->setText(QString("已连接 %1:%2，可以登录").arg(client.serverHost()).arg(client.serverPort()));
    } else {
        ui->connectionStatus->setText(QString("连接失败：%1。请确认服务端已启动、两台电脑在同一局域网，且防火墙允许该端口。").arg(error));
    }
    return ok;
}

void LoginDialog::onLoginClicked()
{
    if (m_busy) return;
    const QString phone = ui->phoneEdit->text().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch()) {
        showWarning("请输入正确的11位手机号!");
        ui->phoneEdit->setFocus();
        return;
    }

    if (!connectServer()) return;
    QScopedValueRollback<bool> guard(m_busy, true);
    setBusy(true);

    bool ok = false;
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUserLogin, QJsonObject{{"phone", phone}}, 5000, &ok);
    setBusy(false);
    if (!ok) {
        showWarning(reply.value("error").toString("登录失败!"));
        ui->phoneEdit->setFocus();
        return;
    }

    QSettings settings;
    if (ui->rememberChk->isChecked())
        settings.setValue("login/lastPhone", encPhone(phone));   // 密文存储
    else
        settings.remove("login/lastPhone");

    ClientSession &s = ClientSession::instance();
    s.userId = reply.value("userId").toInt();
    s.phone = reply.value("phone").toString();
    s.nickname = reply.value("nickname").toString();
    s.balance = reply.value("balance").toDouble();
    s.status = reply.value("status").toInt(UserNormal);

    const bool isNewUser = reply.value("isNew").toBool();
    if (s.status == UserFrozen) {
        QMessageBox::warning(this, "账号已冻结",
                             "您的账号已被冻结，当前无法充值或开始充电。");
    } else {
        QMessageBox::information(this, "提示",
                                 QString(isNewUser ? "注册成功, 欢迎 %1 !"
                                                   : "登录成功, 欢迎 %1 !").arg(s.nickname));
    }
    accept();
}

void LoginDialog::showWarning(const QString &text)
{
    QMessageBox::warning(this, "警告", text, QMessageBox::Yes);
}
