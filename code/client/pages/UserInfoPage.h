#ifndef USERINFOPAGE_H
#define USERINFOPAGE_H

#include "types.h"
#include <QWidget>

class QLabel;
class QBoxLayout;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QVBoxLayout;

class UserInfoPage : public QWidget
{
    Q_OBJECT
public:
    explicit UserInfoPage(QWidget *parent = nullptr);
public slots:
    void refreshPage();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
private slots:
    void onRefresh();
    void onChangeAvatar();
    void onSaveNickname();
    void onRecharge();
    void onAddVehicle();
private:
    void refreshVehicles(bool showError = true);
    void rebuildVehicleCards(const QList<VehicleInfo> &vehicles);
    void openVehicleEditor(const VehicleInfo &vehicle = VehicleInfo());
    void deleteVehicle(int vehicleId, const QString &plateNumber);
    void setDefaultVehicle(int vehicleId);

    QBoxLayout *m_overview = nullptr;
    QBoxLayout *m_settings = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_avatarLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_vehicleCountLabel = nullptr;
    QLineEdit *m_nickEdit = nullptr;
    QPushButton *m_saveNickBtn = nullptr;
    QDoubleSpinBox *m_rechargeSpin = nullptr;
    QPushButton *m_rechargeBtn = nullptr;
    QVBoxLayout *m_vehicleCards = nullptr;
    bool m_silentRefresh = false;
};

#endif
