#ifndef ORDERMANAGEPAGE_H
#define ORDERMANAGEPAGE_H

#include <QHash>
#include <QPair>
#include <QSet>
#include <QWidget>

class QComboBox;
class QPushButton;
class QTableWidget;
class QTabWidget;

// 管理端: 订单管理 + 时段预约管理
// Tab1 订单: 状态筛选 / 强制结束 / 故障退款 / 订单详情
// Tab2 时段预约: 状态展示 / 取消预约
class OrderManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit OrderManagePage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

private slots:
    void refreshOrders();
    void onOrderSelectionChanged();
    void onForceFinish();
    void onRefund();
    void onShowDetail();

    void refreshReservations();
    void onReservationSelectionChanged();
    void onCancelReservation();

private:
    QTabWidget *m_tabs = nullptr;

    // 订单 Tab
    QComboBox *m_statusFilter = nullptr;
    QTableWidget *m_orderTable = nullptr;
    QPushButton *m_forceBtn = nullptr;
    QPushButton *m_refundBtn = nullptr;
    QPushButton *m_detailBtn = nullptr;
    int m_selectedOrderId = -1;
    int m_selectedOrderStatus = -1;
    double m_selectedOrderAmount = 0;
    double m_selectedRefunded = 0;
    bool m_ordersLoaded = false;
    QSet<int> m_knownOrderIds;
    QHash<int, QPair<double, double>> m_previousOrderMetrics;

    // 时段预约 Tab
    QComboBox *m_resFilter = nullptr;
    QTableWidget *m_resTable = nullptr;
    QPushButton *m_cancelResBtn = nullptr;
    int m_selectedResId = -1;
    int m_selectedResStatus = -1;

};

#endif // ORDERMANAGEPAGE_H
