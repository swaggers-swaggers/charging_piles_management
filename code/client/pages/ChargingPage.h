#ifndef CHARGINGPAGE_H
#define CHARGINGPAGE_H

#include "types.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QStackedWidget;
class QTableWidget;

// 电动汽车充电页: 完整"预约—充电—计费—结算"流程
//   进入时查询未完成订单: 有 → 直接进入充电/结算视图(说明书: 强制先结算)
//   无 → 选电站 → 选空闲电桩 → 开始充电(服务端建订单并模拟计费推送) → 结算
class ChargingPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChargingPage(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refreshStations();
    void onStationPicked();
    void onStartCharge();
    void onStopCharge();
    void onPushReceived(const QJsonObject &msg);

private:
    void enterChargingView(const OrderInfo &order);
    void enterSelectView();
    void showSettlement(const QJsonObject &reply);

    // 选择视图
    QComboBox *m_stationCombo;
    QTableWidget *m_pileTable;
    QWidget *m_selectView;

    // 充电视图
    QWidget *m_chargingView;
    QLabel *m_orderLabel;
    QLabel *m_energyLabel;
    QLabel *m_amountLabel;
    QLabel *m_minutesLabel;

    QStackedWidget *m_stack;

    QList<StationInfo> m_stations;
    OrderInfo m_currentOrder;
    bool m_hasOrder = false;
};

#endif // CHARGINGPAGE_H
