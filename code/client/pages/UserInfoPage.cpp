#include "UserInfoPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QBuffer>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
// 默认灰色头像
QPixmap defaultAvatar()
{
    QPixmap pm(96, 96);
    pm.fill(QColor("#B9C2CC"));
    return pm;
}
} // namespace

UserInfoPage::UserInfoPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("用户信息", this);
    title->setObjectName("pageTitle");

    // ---- 头像与基本资料 ----
    QHBoxLayout *profileRow = new QHBoxLayout();
    profileRow->setSpacing(20);

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setFixedSize(96, 96);
    m_avatarLabel->setPixmap(defaultAvatar());
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->setToolTip("点击更换头像");

    QVBoxLayout *infoCol = new QVBoxLayout();
    infoCol->setSpacing(8);
    m_phoneLabel = new QLabel(this);
    m_balanceLabel = new QLabel(this);
    infoCol->addWidget(m_phoneLabel);
    infoCol->addWidget(m_balanceLabel);
    infoCol->addStretch();

    QPushButton *avatarBtn = new QPushButton("更换头像", this);
    connect(avatarBtn, &QPushButton::clicked, this, &UserInfoPage::onChangeAvatar);
    infoCol->addWidget(avatarBtn);

    profileRow->addWidget(m_avatarLabel);
    profileRow->addLayout(infoCol);
    profileRow->addStretch();

    // ---- 昵称修改 ----
    QHBoxLayout *nickRow = new QHBoxLayout();
    QLabel *nickLabel = new QLabel("昵称:", this);
    m_nickEdit = new QLineEdit(this);
    m_nickEdit->setMaxLength(20);
    m_nickEdit->setFixedWidth(240);
    m_saveNickBtn = new QPushButton("保存昵称", this);
    nickRow->addWidget(nickLabel);
    nickRow->addWidget(m_nickEdit);
    nickRow->addWidget(m_saveNickBtn);
    nickRow->addStretch();

    // ---- 充值 ----
    QHBoxLayout *rechargeRow = new QHBoxLayout();
    QLabel *rechargeLabel = new QLabel("充值金额:", this);
    m_rechargeSpin = new QDoubleSpinBox(this);
    m_rechargeSpin->setRange(1.0, 10000.0);
    m_rechargeSpin->setDecimals(2);
    m_rechargeSpin->setSingleStep(10.0);
    m_rechargeSpin->setSuffix(" 元");
    m_rechargeBtn = new QPushButton("充值(模拟支付)", this);
    rechargeRow->addWidget(rechargeLabel);
    rechargeRow->addWidget(m_rechargeSpin);
    rechargeRow->addWidget(m_rechargeBtn);
    rechargeRow->addStretch();

    layout->addWidget(title);
    layout->addLayout(profileRow);
    layout->addSpacing(8);
    layout->addLayout(nickRow);
    layout->addLayout(rechargeRow);
    layout->addStretch();

    connect(m_saveNickBtn, &QPushButton::clicked, this, &UserInfoPage::onSaveNickname);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &UserInfoPage::onRecharge);
}

void UserInfoPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    onRefresh();
}

void UserInfoPage::onRefresh()
{
    ClientSession &s = ClientSession::instance();
    QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqGetUserInfo, QJsonObject{{"userId", s.userId}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "提示", reply.value("error").toString("获取用户信息失败"));
        return;
    }

    s.nickname = reply.value("nickname").toString();
    s.balance = reply.value("balance").toDouble();
    s.avatar = reply.value("avatar").toString();

    m_phoneLabel->setText(QString("手机号: %1").arg(s.phone));
    m_balanceLabel->setText(QString("钱包余额: %1 元").arg(s.balance, 0, 'f', 2));
    m_nickEdit->setText(s.nickname);

    // 头像: base64 → 图片, 失败用默认灰色头像
    if (!s.avatar.isEmpty()) {
        QPixmap pm;
        if (pm.loadFromData(QByteArray::fromBase64(s.avatar.toLatin1()), "PNG"))
            m_avatarLabel->setPixmap(pm.scaled(96, 96, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
    }
}

void UserInfoPage::onChangeAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "选择头像图片", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty())
        return;

    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, "提示", "无法读取所选图片");
        return;
    }
    // 统一缩放为 96x96 PNG 后 base64 上传
    QImage scaled = img.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "PNG");

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUpdateProfile,
        QJsonObject{{"userId", ClientSession::instance().userId},
                    {"avatar", QString::fromLatin1(bytes.toBase64())}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "提示", reply.value("error").toString("头像上传失败"));
        return;
    }
    QMessageBox::information(this, "提示", "头像更新成功!");
    onRefresh();
}

void UserInfoPage::onSaveNickname()
{
    const QString nickname = m_nickEdit->text().trimmed();
    if (nickname.isEmpty()) {
        QMessageBox::warning(this, "提示", "昵称不能为空");
        return;
    }

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUpdateProfile,
        QJsonObject{{"userId", ClientSession::instance().userId},
                    {"nickname", nickname}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "提示", reply.value("error").toString("昵称保存失败"));
        return;
    }
    ClientSession::instance().nickname = nickname;
    QMessageBox::information(this, "提示", "昵称已更新!");
}

void UserInfoPage::onRecharge()
{
    const double amount = m_rechargeSpin->value();
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqRecharge,
        QJsonObject{{"userId", ClientSession::instance().userId},
                    {"amount", amount}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "充值失败", reply.value("error").toString());
        return;
    }
    ClientSession::instance().balance = reply.value("balance").toDouble();
    QMessageBox::information(this, "充值成功",
                             QString("支付成功! 当前余额: %1 元")
                                 .arg(ClientSession::instance().balance, 0, 'f', 2));
    onRefresh();
}
