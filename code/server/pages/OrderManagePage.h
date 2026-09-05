#ifndef ORDERMANAGEPAGE_H
#define ORDERMANAGEPAGE_H

#include <QWidget>

class QComboBox;
class QPushButton;
class QTableWidget;
class QTabWidget;

// 管理端: 订单管理 + 排队/预约管理
// Tab1 订单: 状态筛选 / 强制结束 / 故障退款 / 订单详情
// Tab2 排队预约: 类型筛选 / 取消排队或预约
class OrderManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit OrderManagePage(QWidget *parent = nullptr);

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

    // 排队预约 Tab
    QComboBox *m_resFilter = nullptr;
    QTableWidget *m_resTable = nullptr;
    QPushButton *m_cancelResBtn = nullptr;
    int m_selectedResId = -1;
    int m_selectedResStatus = -1;
};

#endif // ORDERMANAGEPAGE_H
