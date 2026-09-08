#ifndef ORDERHISTORYPAGE_H
#define ORDERHISTORYPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QTimer;

// 用户端"我的订单": 充电订单历史(分页/详情) + 我的排队预约(取消)
class OrderHistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit OrderHistoryPage(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refreshOrders();
    void onPrevPage();
    void onNextPage();
    void onShowDetail();
    void refreshReservations();
    void onCancelReservation();

private:
    QTabWidget *m_tabs;
    QTableWidget *m_orderTable;
    QTableWidget *m_resTable;
    QLabel *m_pageLabel;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_detailBtn;
    QPushButton *m_cancelResBtn;

    int m_page = 0;
    int m_total = 0;
    const int m_pageSize = 15;
    int m_selectedOrderId = -1;
    int m_selectedResId = -1;
    int m_selectedResStatus = -1;
    QTimer *m_autoRefresh = nullptr;   // 每 5 秒自动刷新当前 Tab, 无需手动点刷新按钮
    bool m_autoSilent = false;         // 自动刷新期间失败不弹窗, 避免打断操作
};

#endif // ORDERHISTORYPAGE_H
