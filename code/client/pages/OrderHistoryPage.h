#ifndef ORDERHISTORYPAGE_H
#define ORDERHISTORYPAGE_H

#include "types.h"

#include <QSet>
#include <QWidget>

class QLabel;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

// 用户端“我的订单”：卡片式充电旅程 + 时段预约记录。
class OrderHistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit OrderHistoryPage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

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
    QWidget *createOrderCard(const OrderInfo &order);
    QWidget *createReservationCard(const ReservationInfo &reservation);

    QTabWidget *m_tabs;
    QVBoxLayout *m_orderCards;
    QVBoxLayout *m_reservationCards;
    QLabel *m_orderSummary;
    QLabel *m_reservationSummary;
    QLabel *m_pageLabel;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;

    int m_page = 0;
    int m_total = 0;
    const int m_pageSize = 15;
    int m_selectedOrderId = -1;
    int m_selectedResId = -1;
    int m_selectedResStatus = -1;
    bool m_silentRefresh = false;
    bool m_ordersLoaded = false;
    QSet<int> m_knownOrderIds;
};

#endif // ORDERHISTORYPAGE_H
