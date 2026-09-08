#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVector>

class QLabel;
class QPushButton;
class QGridLayout;
class QShowEvent;
class QHideEvent;
class QResizeEvent;

// 用户端真正的功能首页：用有尺寸层级的 Bento Grid 汇总核心业务入口。
class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

signals:
    void pageRequested(int pageIndex);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPushButton *createBaseCard(const QString &title, const QString &subtitle,
                                int iconType, const QString &tone,
                                const QString &size, int pageIndex,
                                QLabel **badge = nullptr);
    void addAction(QPushButton *card, const QString &text);
    void refreshLocalDetails();
    void loadNetworkDetails();
    void relayoutCards(int availableWidth);

    QGridLayout *m_grid = nullptr;
    QVector<QPushButton *> m_cards;
    int m_layoutColumns = 0;

    QLabel *m_greeting = nullptr;
    QLabel *m_pageSummary = nullptr;
    QLabel *m_heroBalance = nullptr;
    QLabel *m_heroIdle = nullptr;
    QLabel *m_heroUnread = nullptr;
    QLabel *m_connectionStatus = nullptr;
    QLabel *m_endpoint = nullptr;
    QLabel *m_chargeStatus = nullptr;
    QLabel *m_chargeTitle = nullptr;
    QLabel *m_chargeEnergy = nullptr;
    QLabel *m_chargeAmount = nullptr;
    QLabel *m_chargeMinutes = nullptr;
    QLabel *m_accountName = nullptr;
    QLabel *m_accountPhone = nullptr;
    QLabel *m_accountBalance = nullptr;
    QLabel *m_stationSummary = nullptr;
    QVector<QLabel *> m_stationLines;
    QLabel *m_orderSummary = nullptr;
    QVector<QLabel *> m_orderLines;
    QLabel *m_messageSummary = nullptr;
    QVector<QLabel *> m_messageLines;
    bool m_loaded = false;
    bool m_loading = false;
};

#endif // HOMEPAGE_H
