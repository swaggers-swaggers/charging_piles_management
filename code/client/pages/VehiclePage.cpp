#include "VehiclePage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QCheckBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
void clearCards(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

QLabel *makeLabel(const QString &text, const QString &name, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(name);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    return l;
}
} // namespace

VehiclePage::VehiclePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("vehiclePage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 28);
    layout->setSpacing(12);

    layout->addWidget(makeLabel(QStringLiteral("我的车辆"), "pageTitle", this));
    layout->addWidget(makeLabel(QStringLiteral("管理爱车信息，充电补能更快一步"), "pageHint", this));

    auto *toolbar = new QHBoxLayout;
    m_summary = makeLabel(QStringLiteral("正在读取车辆信息…"), "vehicleSummary", this);
    m_addBtn = new QPushButton(QStringLiteral("＋ 添加车辆"), this);
    m_addBtn->setObjectName("primaryBtn");
    toolbar->addWidget(m_summary, 1);
    toolbar->addWidget(m_addBtn);
    layout->addLayout(toolbar);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("vehicleCardScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget(scroll);
    host->setObjectName("vehicleCardsHost");
    m_cards = new QVBoxLayout(host);
    m_cards->setContentsMargins(0, 0, 7, 0);
    m_cards->setSpacing(12);
    m_cards->setAlignment(Qt::AlignTop);
    scroll->setWidget(host);
    layout->addWidget(scroll, 1);

    connect(m_addBtn, &QPushButton::clicked, this, &VehiclePage::onAdd);
}

void VehiclePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &VehiclePage::refresh);
}

void VehiclePage::refreshPage()
{
    m_silentRefresh = true;
    refresh();
    m_silentRefresh = false;
}

void VehiclePage::refresh()
{
    QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqMyVehicles, QJsonObject{{"userId", ClientSession::instance().userId}});
    if (!reply.value("ok").toBool()) {
        if (!m_silentRefresh)
            QMessageBox::warning(this, QStringLiteral("加载失败"), reply.value("error").toString());
        return;
    }

    const QJsonArray vehicles = reply.value("vehicles").toArray();
    clearCards(m_cards);
    int count = 0;
    int defCount = 0;
    for (const QJsonValue &value : vehicles) {
        const VehicleInfo v = VehicleInfo::fromJson(value.toObject());
        ++count;
        if (v.isDefault) ++defCount;
        m_cards->addWidget(createVehicleCard(v));
    }
    if (vehicles.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("还没有绑定车辆\n\n点击右上角「添加车辆」，录入你的第一辆爱车"), this);
        empty->setObjectName("vehicleEmpty");
        empty->setAlignment(Qt::AlignCenter);
        m_cards->addWidget(empty);
    }
    m_summary->setText(QStringLiteral("共 %1 辆车%2").arg(count)
                           .arg(defCount > 0 ? QStringLiteral("   ·   默认车 %1 辆").arg(defCount)
                                             : QString()));
}

QWidget *VehiclePage::createVehicleCard(const VehicleInfo &v)
{
    auto *card = new QFrame(this);
    card->setObjectName("vehicleCard");
    card->setProperty("defaultVehicle", v.isDefault ? "true" : "false");
    auto *body = new QVBoxLayout(card);
    body->setContentsMargins(20, 16, 20, 16);
    body->setSpacing(12);

    auto *top = new QHBoxLayout;
    auto *plate = new QLabel(v.plateNo.isEmpty() ? QStringLiteral("未填车牌") : v.plateNo, card);
    plate->setObjectName("vehiclePlate");
    auto *name = new QLabel(QStringLiteral("%1 · %2")
                                .arg(v.brand.isEmpty() ? QStringLiteral("未知品牌") : v.brand,
                                     v.model.isEmpty() ? QStringLiteral("未知车型") : v.model), card);
    name->setObjectName("vehicleBrand");
    top->addWidget(plate);
    top->addWidget(name, 1);
    if (v.isDefault) {
        auto *badge = new QLabel(QStringLiteral("默认车辆"), card);
        badge->setObjectName("vehicleDefaultBadge");
        top->addWidget(badge);
    }
    body->addLayout(top);

    auto *info = new QLabel(
        QStringLiteral("电池容量  %1 kWh   ·   绑定于 %2")
            .arg(v.batteryCapacity, 0, 'f', 1)
            .arg(v.createTime.isEmpty() ? QStringLiteral("--") : v.createTime), card);
    info->setObjectName("vehicleInfo");
    body->addWidget(info);

    auto *footer = new QHBoxLayout;
    footer->addStretch();
    if (!v.isDefault) {
        auto *setDef = new QPushButton(QStringLiteral("设为默认"), card);
        setDef->setObjectName("vehicleAction");
        footer->addWidget(setDef);
        connect(setDef, &QPushButton::clicked, this, [this, id = v.id] { onSetDefault(id); });
    }
    auto *edit = new QPushButton(QStringLiteral("编辑"), card);
    edit->setObjectName("secondaryBtn");
    auto *del = new QPushButton(QStringLiteral("删除"), card);
    del->setObjectName("vehicleDelete");
    footer->addWidget(edit);
    footer->addWidget(del);
    body->addLayout(footer);

    connect(edit, &QPushButton::clicked, this, [this, id = v.id] { onEdit(id); });
    connect(del, &QPushButton::clicked, this, [this, id = v.id] { onDelete(id); });
    return card;
}

void VehiclePage::onAdd()
{
    onEdit(0);
}

void VehiclePage::onEdit(int vehicleId)
{
    VehicleInfo current;
    if (vehicleId > 0) {
        // 从已有列表中取当前值(编辑时回填)
        QJsonObject reply = TcpClient::instance().request(
            Protocol::ReqMyVehicles, QJsonObject{{"userId", ClientSession::instance().userId}});
        const QJsonArray vehicles = reply.value("vehicles").toArray();
        for (const QJsonValue &value : vehicles) {
            const VehicleInfo v = VehicleInfo::fromJson(value.toObject());
            if (v.id == vehicleId) {
                current = v;
                break;
            }
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(vehicleId > 0 ? QStringLiteral("编辑车辆") : QStringLiteral("添加车辆"));
    dialog.resize(420, 320);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(6);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setVerticalSpacing(12);
    auto *plateEdit = new QLineEdit(current.plateNo, &dialog);
    plateEdit->setPlaceholderText(QStringLiteral("如 京A·12345"));
    auto *brandEdit = new QLineEdit(current.brand, &dialog);
    brandEdit->setPlaceholderText(QStringLiteral("如 比亚迪"));
    auto *modelEdit = new QLineEdit(current.model, &dialog);
    modelEdit->setPlaceholderText(QStringLiteral("如 秦PLUS"));
    auto *capSpin = new QDoubleSpinBox(&dialog);
    capSpin->setRange(0, 500);
    capSpin->setDecimals(1);
    capSpin->setSingleStep(5);
    capSpin->setSuffix(QStringLiteral(" kWh"));
    capSpin->setValue(current.batteryCapacity);
    auto *defCheck = new QCheckBox(QStringLiteral("设为默认车辆"), &dialog);
    defCheck->setChecked(current.isDefault != 0);
    form->addRow(QStringLiteral("车牌号"), plateEdit);
    form->addRow(QStringLiteral("品牌"), brandEdit);
    form->addRow(QStringLiteral("车型"), modelEdit);
    form->addRow(QStringLiteral("电池容量"), capSpin);
    form->addRow(QString(), defCheck);
    layout->addLayout(form);
    layout->addStretch();

    auto *actions = new QHBoxLayout;
    auto *cancel = new QPushButton(QStringLiteral("取消"), &dialog);
    cancel->setObjectName("secondaryBtn");
    auto *ok = new QPushButton(QStringLiteral("保存"), &dialog);
    ok->setObjectName("primaryBtn");
    ok->setDefault(true);
    actions->addStretch();
    actions->addWidget(cancel);
    actions->addWidget(ok);
    layout->addLayout(actions);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    request.insert("id", vehicleId);
    request.insert("plateNo", plateEdit->text().trimmed());
    request.insert("brand", brandEdit->text().trimmed());
    request.insert("model", modelEdit->text().trimmed());
    request.insert("batteryCapacity", capSpin->value());
    request.insert("isDefault", defCheck->isChecked() ? 1 : 0);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqSaveVehicle, request);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), reply.value("error").toString());
        return;
    }
    refresh();
}

void VehiclePage::onDelete(int vehicleId)
{
    if (QMessageBox::question(this, QStringLiteral("删除车辆"),
                              QStringLiteral("确定删除这辆车吗？该操作不可恢复。"))
        != QMessageBox::Yes)
        return;
    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    request.insert("vehicleId", vehicleId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqDeleteVehicle, request);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), reply.value("error").toString());
        return;
    }
    refresh();
}

void VehiclePage::onSetDefault(int vehicleId)
{
    // 复用保存接口, 仅携带 id 与 isDefault, 其余字段由服务端保持不变即可
    // 但 save 会覆盖其余字段, 故先取当前值再提交
    QJsonObject listReply = TcpClient::instance().request(
        Protocol::ReqMyVehicles, QJsonObject{{"userId", ClientSession::instance().userId}});
    VehicleInfo current;
    const QJsonArray vehicles = listReply.value("vehicles").toArray();
    for (const QJsonValue &value : vehicles) {
        const VehicleInfo v = VehicleInfo::fromJson(value.toObject());
        if (v.id == vehicleId) {
            current = v;
            break;
        }
    }
    if (current.id == 0)
        return;

    QJsonObject request;
    request.insert("userId", ClientSession::instance().userId);
    request.insert("id", current.id);
    request.insert("plateNo", current.plateNo);
    request.insert("brand", current.brand);
    request.insert("model", current.model);
    request.insert("batteryCapacity", current.batteryCapacity);
    request.insert("isDefault", 1);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqSaveVehicle, request);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), reply.value("error").toString());
        return;
    }
    refresh();
}
