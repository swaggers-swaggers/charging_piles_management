#ifndef CHARGINGPAGE_H
#define CHARGINGPAGE_H

#include "types.h"

#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;

// 环形目标进度控件: 外环为目标完成度, 中心显示主/副文本
class ChargeRingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChargeRingWidget(QWidget *parent = nullptr);
    // progress <0 表示无明确目标(不画进度弧)
    void setProgress(double progress);
    void setCenterText(const QString &big, const QString &small);
    QSize sizeHint() const override { return QSize(220, 220); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_progress = -1.0;
    QString m_big;
    QString m_small;
};

// 电动汽车充电页: 选桩(卡片) → 充电设置(目标/冻结) → 仪表盘充电 → 结算
// 同时承载现场排队与时段预约的等待视图, 服务端推送实时驱动界面
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
    void onStopCharge();
    void onPushReceived(const QJsonObject &msg);
    void onCancelWaiting();

private:
    void buildSelectView();
    void buildChargingView();
    void buildWaitingView();

    void rebuildPileCards();
    void openChargeSetup(int pileId);
    void openAppointDialog(int pileId);
    void joinQueue(int pileId);
    void doStart(int pileId, int targetType, double targetValue);
    void enterChargingView(const OrderInfo &order);
    void enterSelectView();
    void enterWaitingView(const ReservationInfo &r);
    void refreshWaiting();
    void showSettlement(const OrderInfo &order, double balance);

    QStackedWidget *m_stack;

    // ---- 选桩视图 ----
    QComboBox *m_stationCombo;
    QLabel *m_stationInfo;
    QScrollArea *m_cardScroll;
    QWidget *m_cardHost;
    QGridLayout *m_cardGrid;
    QWidget *m_selectView;

    // ---- 充电视图 ----
    QWidget *m_chargingView;
    ChargeRingWidget *m_ring;
    QLabel *m_orderTitle;
    QLabel *m_energyVal;
    QLabel *m_amountVal;
    QLabel *m_minutesVal;
    QLabel *m_priceHint;

    // ---- 排队/预约等待视图 ----
    QWidget *m_waitingView;
    QLabel *m_waitTitle;
    QLabel *m_waitDesc;
    QPushButton *m_cancelWaitBtn;

    QList<StationInfo> m_stations;
    QList<PileInfo> m_piles;
    OrderInfo m_currentOrder;
    bool m_hasOrder = false;

    // 当前等待中的排队/预约
    int m_waitingId = -1;
    int m_waitingPileId = -1;
    int m_waitingType = ReserveQueue;
};

#endif // CHARGINGPAGE_H
