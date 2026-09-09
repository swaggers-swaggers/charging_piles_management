#include "UserInfoPage.h"

#include "ClientSession.h"
#include "IconFactory.h"
#include "network/TcpClient.h"
#include "protocol.h"

#include <QBuffer>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QPixmap defaultAvatar()
{
    return IconFactory::icon(IconFactory::IconUser, QColor("#237653"), 96).pixmap(96, 96);
}

QLabel *label(const QString &text, const QString &name, QWidget *parent)
{
    auto *result = new QLabel(text, parent);
    result->setObjectName(name);
    result->setWordWrap(true);
    result->setTextFormat(Qt::PlainText);
    return result;
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        if (item->layout()) clearLayout(item->layout());
        delete item;
    }
}

class VehicleEditorDialog : public QDialog
{
public:
    VehicleEditorDialog(const VehicleInfo &vehicle, QWidget *parent)
        : QDialog(parent), m_vehicleId(vehicle.id)
    {
        setObjectName("vehicleEditorDialog");
        setWindowTitle(vehicle.id ? "编辑车辆" : "添加车辆");
        setMinimumWidth(430);
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(26, 24, 26, 24); root->setSpacing(13);
        root->addWidget(label(vehicle.id ? "更新车辆档案" : "把爱车加入车库", "vehicleDialogTitle", this));
        root->addWidget(label("车辆信息仅用于充电服务和行程展示", "vehicleDialogHint", this));
        m_plate = new QLineEdit(this); m_plate->setObjectName("vehiclePlateEdit");
        m_plate->setMaxLength(10); m_plate->setPlaceholderText("例如：京A·12345"); m_plate->setText(vehicle.plateNumber);
        root->addWidget(label("车牌号", "vehicleFieldLabel", this)); root->addWidget(m_plate);
        m_model = new QLineEdit(this); m_model->setObjectName("vehicleModelEdit");
        m_model->setMaxLength(40); m_model->setPlaceholderText("例如：Tesla Model 3"); m_model->setText(vehicle.brandModel);
        root->addWidget(label("品牌与车型", "vehicleFieldLabel", this)); root->addWidget(m_model);
        auto *specs = new QHBoxLayout;
        auto *typeColumn = new QVBoxLayout; typeColumn->addWidget(label("能源类型", "vehicleFieldLabel", this));
        m_type = new QComboBox(this); m_type->setObjectName("vehicleEnergyType"); m_type->addItems({"纯电", "插电混动", "增程"});
        const int selected = m_type->findText(vehicle.energyType); if (selected >= 0) m_type->setCurrentIndex(selected);
        typeColumn->addWidget(m_type);
        auto *capacityColumn = new QVBoxLayout; capacityColumn->addWidget(label("电池容量", "vehicleFieldLabel", this));
        m_capacity = new QDoubleSpinBox(this); m_capacity->setObjectName("vehicleCapacitySpin");
        m_capacity->setRange(1, 300); m_capacity->setDecimals(1); m_capacity->setSuffix(" kWh");
        m_capacity->setValue(vehicle.batteryCapacity > 0 ? vehicle.batteryCapacity : 60);
        capacityColumn->addWidget(m_capacity); specs->addLayout(typeColumn, 1); specs->addLayout(capacityColumn, 1);
        root->addLayout(specs); root->addSpacing(4);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
        buttons->button(QDialogButtonBox::Cancel)->setText("取消");
        buttons->button(QDialogButtonBox::Save)->setText(vehicle.id ? "保存修改" : "添加车辆");
        buttons->button(QDialogButtonBox::Save)->setObjectName("primaryBtn");
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (plateNumber().size() < 2 || brandModel().isEmpty()) {
                QMessageBox::warning(this, "信息未完整", "请填写车牌号和品牌车型"); return;
            }
            accept();
        });
        root->addWidget(buttons);
    }
    int vehicleId() const { return m_vehicleId; }
    QString plateNumber() const { return m_plate->text().trimmed().toUpper(); }
    QString brandModel() const { return m_model->text().trimmed(); }
    QString energyType() const { return m_type->currentText(); }
    double batteryCapacity() const { return m_capacity->value(); }
private:
    int m_vehicleId;
    QLineEdit *m_plate;
    QLineEdit *m_model;
    QComboBox *m_type;
    QDoubleSpinBox *m_capacity;
};
}

UserInfoPage::UserInfoPage(QWidget *parent) : QWidget(parent)
{
    setObjectName("accountPage");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *page = new QVBoxLayout(this); page->setContentsMargins(28, 24, 28, 32); page->setSpacing(18);
    auto *titleRow = new QHBoxLayout; auto *titles = new QVBoxLayout; titles->setSpacing(3);
    titles->addWidget(label("我的账户", "pageTitle", this));
    titles->addWidget(label("个人资料、钱包与爱车，都在这里井然有序", "pageHint", this));
    titleRow->addLayout(titles); titleRow->addStretch(); titleRow->addWidget(label("个人中心", "accountTopBadge", this), 0, Qt::AlignTop);
    page->addLayout(titleRow);

    auto *profile = new QFrame(this); profile->setObjectName("accountProfile");
    auto *profileRow = new QHBoxLayout(profile); profileRow->setContentsMargins(24, 22, 24, 22); profileRow->setSpacing(20);
    m_avatarLabel = new QLabel(profile); m_avatarLabel->setObjectName("accountAvatar"); m_avatarLabel->setFixedSize(76, 76); m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setPixmap(defaultAvatar().scaled(58, 58, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *identity = new QVBoxLayout; identity->setSpacing(5); identity->addWidget(label("已认证用户", "accountStatus", profile));
    m_nameLabel = label(ClientSession::instance().nickname, "accountName", profile);
    m_phoneLabel = label(ClientSession::instance().phone, "pageHint", profile);
    identity->addWidget(m_nameLabel); identity->addWidget(m_phoneLabel); identity->addStretch();
    auto *avatarButton = new QPushButton("更换头像", profile); avatarButton->setObjectName("secondaryBtn");
    profileRow->addWidget(m_avatarLabel); profileRow->addLayout(identity, 1); profileRow->addWidget(avatarButton);
    connect(avatarButton, &QPushButton::clicked, this, &UserInfoPage::onChangeAvatar);

    auto *wallet = new QFrame(this); wallet->setObjectName("accountWallet");
    auto *walletLayout = new QVBoxLayout(wallet); walletLayout->setContentsMargins(24, 22, 24, 22); walletLayout->setSpacing(7);
    auto *walletHead = new QHBoxLayout; walletHead->addWidget(label("充电钱包", "walletCaption", wallet)); walletHead->addStretch();
    walletHead->addWidget(label("可随时充值", "walletPill", wallet)); walletLayout->addLayout(walletHead);
    m_balanceLabel = label(QString::number(ClientSession::instance().balance, 'f', 2), "walletAmount", wallet);
    walletLayout->addWidget(m_balanceLabel); walletLayout->addWidget(label("元  ·  按实际充电量实时扣除", "walletCaption", wallet)); walletLayout->addStretch();
    m_overview = new QBoxLayout(QBoxLayout::LeftToRight); m_overview->setSpacing(18);
    m_overview->addWidget(profile, 3); m_overview->addWidget(wallet, 2); page->addLayout(m_overview);

    auto *details = new QFrame(this); details->setObjectName("accountSection");
    auto *detailsLayout = new QVBoxLayout(details); detailsLayout->setContentsMargins(24, 20, 24, 22); detailsLayout->setSpacing(12);
    detailsLayout->addWidget(label("个人资料", "sectionTitle", details)); detailsLayout->addWidget(label("设置一个方便识别的称呼", "pageHint", details));
    auto *nicknameRow = new QHBoxLayout; m_nickEdit = new QLineEdit(details); m_nickEdit->setObjectName("nickEdit");
    m_nickEdit->setMaxLength(20); m_nickEdit->setPlaceholderText("输入你的昵称");
    m_saveNickBtn = new QPushButton("保存昵称", details); m_saveNickBtn->setObjectName("primaryBtn");
    nicknameRow->addWidget(m_nickEdit, 1); nicknameRow->addWidget(m_saveNickBtn); detailsLayout->addLayout(nicknameRow); detailsLayout->addStretch();

    auto *recharge = new QFrame(this); recharge->setObjectName("accountSection");
    auto *rechargeLayout = new QVBoxLayout(recharge); rechargeLayout->setContentsMargins(24, 20, 24, 22); rechargeLayout->setSpacing(12);
    rechargeLayout->addWidget(label("钱包充值", "sectionTitle", recharge)); rechargeLayout->addWidget(label("选择常用金额，或输入自定义金额", "pageHint", recharge));
    auto *amountRow = new QHBoxLayout;
    m_rechargeSpin = new QDoubleSpinBox(recharge); m_rechargeSpin->setObjectName("rechargeSpin");
    m_rechargeSpin->setRange(1, 10000); m_rechargeSpin->setDecimals(2); m_rechargeSpin->setValue(100); m_rechargeSpin->setPrefix("¥ ");
    for (int amount : {50, 100, 200}) {
        auto *button = new QPushButton(QString("%1 元").arg(amount), recharge); button->setObjectName("amountPreset"); button->setCheckable(true); button->setChecked(amount == 100);
        amountRow->addWidget(button, 1); connect(button, &QPushButton::clicked, this, [this, amount] { m_rechargeSpin->setValue(amount); });
        connect(m_rechargeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), button, [button, amount](double value) { button->setChecked(value == amount); });
    }
    rechargeLayout->addLayout(amountRow); auto *payRow = new QHBoxLayout;
    m_rechargeBtn = new QPushButton("确认充值", recharge); m_rechargeBtn->setObjectName("successBtn");
    payRow->addWidget(m_rechargeSpin, 1); payRow->addWidget(m_rechargeBtn); rechargeLayout->addLayout(payRow);
    rechargeLayout->addWidget(label("模拟支付 · 不产生真实扣款", "pageHint", recharge));
    m_settings = new QBoxLayout(QBoxLayout::LeftToRight); m_settings->setSpacing(18);
    m_settings->addWidget(details, 1); m_settings->addWidget(recharge, 1); page->addLayout(m_settings);

    auto *garage = new QFrame(this); garage->setObjectName("vehicleGarage");
    auto *garageLayout = new QVBoxLayout(garage); garageLayout->setContentsMargins(24, 22, 24, 24); garageLayout->setSpacing(14);
    auto *garageHead = new QHBoxLayout; auto *garageTitles = new QVBoxLayout; garageTitles->setSpacing(3);
    garageTitles->addWidget(label("我的车辆", "sectionTitle", garage)); m_vehicleCountLabel = label("正在读取车库…", "pageHint", garage); garageTitles->addWidget(m_vehicleCountLabel);
    auto *addVehicle = new QPushButton("＋ 添加车辆", garage); addVehicle->setObjectName("addVehicleBtn");
    garageHead->addLayout(garageTitles); garageHead->addStretch(); garageHead->addWidget(addVehicle); garageLayout->addLayout(garageHead);
    m_vehicleCards = new QVBoxLayout; m_vehicleCards->setSpacing(10); garageLayout->addLayout(m_vehicleCards); page->addWidget(garage); page->addStretch();
    connect(m_saveNickBtn, &QPushButton::clicked, this, &UserInfoPage::onSaveNickname);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &UserInfoPage::onRecharge);
    connect(addVehicle, &QPushButton::clicked, this, &UserInfoPage::onAddVehicle);
}

void UserInfoPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const auto direction = width() < 760 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    m_overview->setDirection(direction); m_settings->setDirection(direction);
}

void UserInfoPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event); QTimer::singleShot(0, this, &UserInfoPage::onRefresh);
}

void UserInfoPage::refreshPage() { m_silentRefresh = true; onRefresh(); m_silentRefresh = false; }

void UserInfoPage::onRefresh()
{
    ClientSession &session = ClientSession::instance();
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqGetUserInfo);
    if (!reply.value("ok").toBool()) {
        if (!m_silentRefresh) QMessageBox::warning(this, "提示", reply.value("error").toString("获取用户信息失败"));
        return;
    }
    session.nickname = reply.value("nickname").toString(); session.balance = reply.value("balance").toDouble(); session.avatar = reply.value("avatar").toString();
    m_phoneLabel->setText(QString("手机号  %1").arg(session.phone)); m_balanceLabel->setText(QString::number(session.balance, 'f', 2));
    m_nameLabel->setText(session.nickname.isEmpty() ? "充电用户" : session.nickname);
    if (!m_nickEdit->hasFocus() && !m_nickEdit->isModified()) m_nickEdit->setText(session.nickname);
    m_avatarLabel->setPixmap(defaultAvatar().scaled(58, 58, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (!session.avatar.isEmpty()) { QPixmap image; if (image.loadFromData(QByteArray::fromBase64(session.avatar.toLatin1()), "PNG")) m_avatarLabel->setPixmap(image.scaled(66, 66, Qt::KeepAspectRatio, Qt::SmoothTransformation)); }
    refreshVehicles(!m_silentRefresh);
}

void UserInfoPage::refreshVehicles(bool showError)
{
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqVehicleList);
    if (!reply.value("ok").toBool()) {
        m_vehicleCountLabel->setText("车库暂时无法读取");
        if (showError) QMessageBox::warning(this, "提示", reply.value("error").toString("获取车辆失败")); return;
    }
    QList<VehicleInfo> vehicles;
    for (const QJsonValue &value : reply.value("vehicles").toArray()) vehicles.append(VehicleInfo::fromJson(value.toObject()));
    rebuildVehicleCards(vehicles);
}

void UserInfoPage::rebuildVehicleCards(const QList<VehicleInfo> &vehicles)
{
    clearLayout(m_vehicleCards);
    m_vehicleCountLabel->setText(vehicles.isEmpty() ? "还没有车辆，添加后可统一管理" : QString("%1 辆爱车 · 默认车辆会优先用于充电服务").arg(vehicles.size()));
    if (vehicles.isEmpty()) {
        auto *empty = label("你的私人车库还是空的\n添加车辆后，车型与电池信息会在这里集中呈现", "vehicleEmpty", this);
        empty->setAlignment(Qt::AlignCenter); m_vehicleCards->addWidget(empty); return;
    }
    for (const VehicleInfo &vehicle : vehicles) {
        auto *card = new QFrame(this); card->setObjectName("vehicleCard"); card->setProperty("defaultVehicle", vehicle.isDefault);
        auto *row = new QHBoxLayout(card); row->setContentsMargins(18, 16, 16, 16); row->setSpacing(16);
        auto *icon = label("EV", "vehicleIcon", card); icon->setAlignment(Qt::AlignCenter);
        auto *identity = new QVBoxLayout; identity->setSpacing(5); auto *nameRow = new QHBoxLayout;
        nameRow->addWidget(label(vehicle.plateNumber, "vehiclePlate", card));
        if (vehicle.isDefault) nameRow->addWidget(label("默认车辆", "vehicleDefaultBadge", card));
        nameRow->addStretch(); identity->addLayout(nameRow); identity->addWidget(label(vehicle.brandModel, "vehicleModel", card));
        identity->addWidget(label(QString("%1  ·  %2 kWh").arg(vehicle.energyType).arg(vehicle.batteryCapacity, 0, 'f', 1), "vehicleMeta", card));
        auto *actions = new QHBoxLayout; actions->setSpacing(6);
        if (!vehicle.isDefault) { auto *makeDefault = new QPushButton("设为默认", card); makeDefault->setObjectName("vehicleTextButton"); connect(makeDefault, &QPushButton::clicked, this, [this, vehicle] { setDefaultVehicle(vehicle.id); }); actions->addWidget(makeDefault); }
        auto *edit = new QPushButton("编辑", card); edit->setObjectName("vehicleTextButton");
        auto *remove = new QPushButton("删除", card); remove->setObjectName("vehicleDeleteButton");
        connect(edit, &QPushButton::clicked, this, [this, vehicle] { openVehicleEditor(vehicle); });
        connect(remove, &QPushButton::clicked, this, [this, vehicle] { deleteVehicle(vehicle.id, vehicle.plateNumber); });
        actions->addWidget(edit); actions->addWidget(remove); row->addWidget(icon); row->addLayout(identity, 1); row->addLayout(actions); m_vehicleCards->addWidget(card);
    }
}

void UserInfoPage::onAddVehicle() { openVehicleEditor(); }

void UserInfoPage::openVehicleEditor(const VehicleInfo &vehicle)
{
    VehicleEditorDialog dialog(vehicle, this); if (dialog.exec() != QDialog::Accepted) return;
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqSaveVehicle, QJsonObject{{"vehicleId", dialog.vehicleId()}, {"plateNumber", dialog.plateNumber()}, {"brandModel", dialog.brandModel()}, {"energyType", dialog.energyType()}, {"batteryCapacity", dialog.batteryCapacity()}});
    if (!reply.value("ok").toBool()) { QMessageBox::warning(this, "保存失败", reply.value("error").toString()); return; }
    refreshVehicles(false);
}

void UserInfoPage::deleteVehicle(int vehicleId, const QString &plateNumber)
{
    if (QMessageBox::question(this, "删除车辆", QString("确定从车库中删除 %1 吗？").arg(plateNumber)) != QMessageBox::Yes) return;
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqDeleteVehicle, QJsonObject{{"vehicleId", vehicleId}});
    if (!reply.value("ok").toBool()) { QMessageBox::warning(this, "删除失败", reply.value("error").toString()); return; }
    refreshVehicles(false);
}

void UserInfoPage::setDefaultVehicle(int vehicleId)
{
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqSetDefaultVehicle, QJsonObject{{"vehicleId", vehicleId}});
    if (!reply.value("ok").toBool()) { QMessageBox::warning(this, "设置失败", reply.value("error").toString()); return; }
    refreshVehicles(false);
}

void UserInfoPage::onChangeAvatar()
{
    const QString path = QFileDialog::getOpenFileName(this, "选择头像图片", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp)"); if (path.isEmpty()) return;
    QImage image(path); if (image.isNull()) { QMessageBox::warning(this, "提示", "无法读取所选图片"); return; }
    const QImage scaled = image.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); scaled.save(&buffer, "PNG");
    const QString encoded = QString::fromLatin1(bytes.toBase64());
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqUpdateProfile, QJsonObject{{"avatar", encoded}});
    if (!reply.value("ok").toBool()) { QMessageBox::warning(this, "提示", reply.value("error").toString("头像上传失败")); return; }
    ClientSession::instance().avatar = encoded; m_avatarLabel->setPixmap(QPixmap::fromImage(scaled).scaled(66, 66, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void UserInfoPage::onSaveNickname()
{
    const QString nickname = m_nickEdit->text().trimmed(); if (nickname.isEmpty()) { QMessageBox::warning(this, "提示", "昵称不能为空"); return; }
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqUpdateProfile, QJsonObject{{"nickname", nickname}});
    if (!reply.value("ok").toBool()) { QMessageBox::warning(this, "提示", reply.value("error").toString("昵称保存失败")); return; }
    ClientSession::instance().nickname = nickname; m_nameLabel->setText(nickname);
}

void UserInfoPage::onRecharge()
{
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqRecharge, QJsonObject{{"amount", m_rechargeSpin->value()}});
    if (!reply.value("ok").toBool()) { QMessageBox::warning(this, "充值失败", reply.value("error").toString()); return; }
    ClientSession::instance().balance = reply.value("balance").toDouble(); m_balanceLabel->setText(QString::number(ClientSession::instance().balance, 'f', 2));
}
