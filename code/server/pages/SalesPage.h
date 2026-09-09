#ifndef SALESPAGE_H
#define SALESPAGE_H

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTcpServer;
class QTimer;
class RevenueChart;

// 管理端首页：营收、设备、用户与局域网连接信息组成非对称 Bento 概览。
class SalesPage : public QWidget
{
    Q_OBJECT

public:
    explicit SalesPage(QWidget *parent = nullptr);
    void showConnectionInfo(QTcpServer *server, const QString &serverInfo,
                            const QString &webUrl);

public slots:
    void refreshPage();

signals:
    void pageRequested(int pageIndex);
    void openWebRequested();

private slots:
    void refresh();
    void refreshConnectionInfo();

private:
    QLabel *m_todayVal = nullptr;
    QLabel *m_monthVal = nullptr;
    QLabel *m_totalVal = nullptr;
    QLabel *m_pileIdle = nullptr;
    QLabel *m_pileBusy = nullptr;
    QLabel *m_pileFault = nullptr;
    QLabel *m_stationCount = nullptr;
    QLabel *m_stationLeader = nullptr;
    QLabel *m_stationPileDetail = nullptr;
    QLabel *m_stationPriceDetail = nullptr;
    QLabel *m_orderCount = nullptr;
    QLabel *m_orderActive = nullptr;
    QLabel *m_orderCompleted = nullptr;
    QLabel *m_orderEnergy = nullptr;
    QLabel *m_userCount = nullptr;
    QLabel *m_userStatus = nullptr;
    QLabel *m_userBalance = nullptr;
    QLabel *m_userNewest = nullptr;
    QLabel *m_adminName = nullptr;
    QLabel *m_serverSummary = nullptr;
    QLabel *m_lanStatus = nullptr;
    QLabel *m_addressMeta = nullptr;
    QLabel *m_otherAddresses = nullptr;
    QLabel *m_localAddress = nullptr;
    QLabel *m_webStatus = nullptr;
    QPushButton *m_copyAddressButton = nullptr;
    QComboBox *m_rangeCombo = nullptr;
    QLabel *m_addressesText = nullptr;
    RevenueChart *m_chart = nullptr;
    QTcpServer *m_tcpServer = nullptr;
    QTimer *m_connectionTimer = nullptr;
    QString m_webUrl;
    QString m_primaryEndpoint;
};

#endif // SALESPAGE_H
