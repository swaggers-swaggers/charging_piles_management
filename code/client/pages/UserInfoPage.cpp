#include "UserInfoPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QBuffer>
#include <QFrame>
#include <QResizeEvent>
#include "IconFactory.h"
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// 默认灰色头像
QPixmap defaultAvatar()
{
    QPixmap pm = IconFactory::icon(IconFactory::IconUser, QColor("#237653"), 96).pixmap(96, 96);
    return pm;
}
} // namespace

UserInfoPage::UserInfoPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("accountPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(18);
    auto label = [](const QString &text, const QString &name, QWidget *parent) {
        auto *l = new QLabel(text, parent); l->setObjectName(name);
        l->setWordWrap(true); l->setTextFormat(Qt::PlainText); return l;
    };
    layout->addWidget(label("我的账户", "pageTitle", this));
    layout->addWidget(label("管理个人资料，为下一次出发做好准备", "pageHint", this));

    auto *profile = new QFrame(this); profile->setObjectName("accountProfile");
    auto *profileRow = new QHBoxLayout(profile);
    profileRow->setContentsMargins(24, 22, 24, 22); profileRow->setSpacing(20);
    m_avatarLabel = new QLabel(profile); m_avatarLabel->setObjectName("accountAvatar");
    m_avatarLabel->setFixedSize(72,72); m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setPixmap(defaultAvatar().scaled(56,56,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    auto *identity = new QVBoxLayout;
    m_nameLabel = label(ClientSession::instance().nickname, "accountName", profile);
    m_phoneLabel = label(ClientSession::instance().phone, "pageHint", profile);
    identity->addWidget(m_nameLabel); identity->addWidget(m_phoneLabel);
    auto *avatarBtn = new QPushButton("更换头像", profile); avatarBtn->setObjectName("secondaryBtn");
    profileRow->addWidget(m_avatarLabel); profileRow->addLayout(identity,1); profileRow->addWidget(avatarBtn);
    m_overview = new QBoxLayout(QBoxLayout::LeftToRight);
    m_overview->setSpacing(18);
    m_overview->addWidget(profile, 1);
    connect(avatarBtn,&QPushButton::clicked,this,&UserInfoPage::onChangeAvatar);

    auto *wallet = new QFrame(this); wallet->setObjectName("accountWallet");
    auto *walletLayout = new QVBoxLayout(wallet); walletLayout->setContentsMargins(24,22,24,22);
    walletLayout->setSpacing(8);
    walletLayout->addWidget(label("可用余额 · 元", "walletCaption", wallet));
    m_balanceLabel = label(QString::number(ClientSession::instance().balance,'f',2), "walletAmount", wallet);
    walletLayout->addWidget(m_balanceLabel);
    walletLayout->addWidget(label("充电预授权冻结金额不计入可用余额", "walletCaption", wallet));
    m_overview->addWidget(wallet, 1);
    layout->addLayout(m_overview);

    auto *details = new QFrame(this); details->setObjectName("accountSection");
    auto *detailsLayout = new QVBoxLayout(details); detailsLayout->setContentsMargins(24,20,24,22);
    detailsLayout->setSpacing(12);
    detailsLayout->addWidget(label("个人资料", "sectionTitle", details));
    detailsLayout->addWidget(label("昵称 · 最多 20 个字符", "pageHint", details));
    auto *nickRow = new QHBoxLayout;
    m_nickEdit = new QLineEdit(details); m_nickEdit->setObjectName("nickEdit");
    m_nickEdit->setMaxLength(20); m_nickEdit->setPlaceholderText("输入你的昵称");
    m_nickEdit->setAccessibleName("昵称"); m_nickEdit->setMinimumWidth(100);
    m_saveNickBtn = new QPushButton("保存昵称", details); m_saveNickBtn->setObjectName("primaryBtn");
    nickRow->addWidget(m_nickEdit,1); nickRow->addWidget(m_saveNickBtn);
    detailsLayout->addLayout(nickRow); detailsLayout->addStretch();
    m_settings = new QBoxLayout(QBoxLayout::LeftToRight); m_settings->setSpacing(18);
    m_settings->addWidget(details,1);

    auto *recharge = new QFrame(this); recharge->setObjectName("accountSection");
    auto *rechargeLayout = new QVBoxLayout(recharge); rechargeLayout->setContentsMargins(24,20,24,22);
    rechargeLayout->setSpacing(12);
    rechargeLayout->addWidget(label("钱包充值", "sectionTitle", recharge));
    rechargeLayout->addWidget(label("选择常用金额，或输入自定义金额", "pageHint", recharge));
    auto *amountRow = new QHBoxLayout;
    m_rechargeSpin = new QDoubleSpinBox(recharge); m_rechargeSpin->setObjectName("rechargeSpin");
    m_rechargeSpin->setRange(1,10000); m_rechargeSpin->setDecimals(2);
    m_rechargeSpin->setSingleStep(10); m_rechargeSpin->setValue(100);
    m_rechargeSpin->setPrefix("¥ "); m_rechargeSpin->setAccessibleName("充值金额");
    for (int amount : {50,100,200}) {
        auto *b = new QPushButton(QString("%1 元").arg(amount), recharge);
        b->setObjectName("amountPreset"); b->setCheckable(true);
        b->setChecked(amount == 100); amountRow->addWidget(b,1);
        connect(b,&QPushButton::clicked,this,[this,amount,b]{m_rechargeSpin->setValue(amount); b->setChecked(true);});
        connect(m_rechargeSpin,qOverload<double>(&QDoubleSpinBox::valueChanged),b,[b,amount](double value){b->setChecked(value==amount);});
    }
    rechargeLayout->addLayout(amountRow);
    auto *payRow = new QHBoxLayout;
    m_rechargeBtn = new QPushButton("确认充值", recharge); m_rechargeBtn->setObjectName("successBtn");
    payRow->addWidget(m_rechargeSpin,1); payRow->addWidget(m_rechargeBtn);
    rechargeLayout->addLayout(payRow);
    rechargeLayout->addWidget(label("模拟支付 · 不产生真实扣款", "pageHint", recharge));
    m_settings->addWidget(recharge,1); layout->addLayout(m_settings); layout->addStretch();
    connect(m_saveNickBtn,&QPushButton::clicked,this,&UserInfoPage::onSaveNickname);
    connect(m_rechargeBtn,&QPushButton::clicked,this,&UserInfoPage::onRecharge);
}

void UserInfoPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const auto direction = width() < 760 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    m_overview->setDirection(direction);
    m_settings->setDirection(direction);
}

void UserInfoPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到界面显示完成后再请求, 避免同步网络请求阻塞主窗口切换
    QTimer::singleShot(0, this, &UserInfoPage::onRefresh);
}

void UserInfoPage::refreshPage()
{
    m_silentRefresh = true;
    onRefresh();
    m_silentRefresh = false;
}

void UserInfoPage::onRefresh()
{
    ClientSession &s = ClientSession::instance();
    QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqGetUserInfo, QJsonObject{{"userId", s.userId}});
    if (!reply.value("ok").toBool()) {
        if (!m_silentRefresh)
            QMessageBox::warning(this, "提示", reply.value("error").toString("获取用户信息失败"));
        return;
    }

    s.nickname = reply.value("nickname").toString();
    s.balance = reply.value("balance").toDouble();
    s.avatar = reply.value("avatar").toString();

    m_phoneLabel->setText(QString("手机号: %1").arg(s.phone));
    m_balanceLabel->setText(QString::number(s.balance, 'f', 2));
    m_nameLabel->setText(s.nickname.isEmpty() ? "充电用户" : s.nickname);
    if (!m_nickEdit->hasFocus() && !m_nickEdit->isModified())
        m_nickEdit->setText(s.nickname);

    m_avatarLabel->setPixmap(defaultAvatar().scaled(56,56,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    // 头像: base64 → 图片, 失败用默认灰色头像
    if (!s.avatar.isEmpty()) {
        QPixmap pm;
        if (pm.loadFromData(QByteArray::fromBase64(s.avatar.toLatin1()), "PNG"))
            m_avatarLabel->setPixmap(pm.scaled(64, 64, Qt::KeepAspectRatio,
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

    const QString avatarBase64 = QString::fromLatin1(bytes.toBase64());
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUpdateProfile,
        QJsonObject{{"userId", ClientSession::instance().userId},
                    {"avatar", avatarBase64}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, "提示", reply.value("error").toString("头像上传失败"));
        return;
    }

    // 提交成功后立即更新本地会话与头像显示, 无需重新进入页面
    ClientSession::instance().avatar = avatarBase64;
    m_avatarLabel->setPixmap(QPixmap::fromImage(scaled).scaled(64,64,Qt::KeepAspectRatio,Qt::SmoothTransformation));

    QMessageBox::information(this, "提示", "提交成功");
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
    m_nameLabel->setText(nickname);
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
