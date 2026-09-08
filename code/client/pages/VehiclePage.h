#ifndef VEHICLEPAGE_H
#define VEHICLEPAGE_H

#include "types.h"

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;

// 用户端“我的车辆”：爱车卡片 + 添加/编辑/删除/设默认, 全部经 Socket 由服务端落库。
class VehiclePage : public QWidget
{
    Q_OBJECT

public:
    explicit VehiclePage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refresh();
    void onAdd();
    void onEdit(int vehicleId);
    void onDelete(int vehicleId);
    void onSetDefault(int vehicleId);

private:
    QWidget *createVehicleCard(const VehicleInfo &v);

    QVBoxLayout *m_cards = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_addBtn = nullptr;
    bool m_silentRefresh = false;
};

#endif // VEHICLEPAGE_H
