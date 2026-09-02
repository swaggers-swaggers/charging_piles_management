#include "LoginDialog.h"
#include "ui_LoginDialog.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QFile>
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>

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
    setFixedSize(440, 560);

    loadStyleSheet();

    // 手机号输入校验: 只允许 1 开头的数字(允许输入过程中的中间状态)
    ui->phoneEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("^1\\d{0,10}$"), ui->phoneEdit));

    // 服务端地址提示
    ui->hintLabel->setText(QString("需要先启动服务端 ChargingServer (%1:%2)")
                               .arg(Protocol::serverHost())
                               .arg(Protocol::serverPort()));

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
    connect(ui->loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->phoneEdit, &QLineEdit::returnPressed, ui->loginBtn, &QPushButton::click);
    connect(ui->exitBtn, &QPushButton::clicked, this, &QWidget::close);
}

void LoginDialog::onLoginClicked()
{
    const QString phone = ui->phoneEdit->text().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch()) {
        showWarning("请输入正确的11位手机号!");
        ui->phoneEdit->setFocus();
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
        ui->phoneEdit->clear();
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
