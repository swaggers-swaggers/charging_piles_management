#ifndef CONSUMPTIONPAGE_H
#define CONSUMPTIONPAGE_H

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QGridLayout;

// 用户端“我的历史消费”：累计消费总览 + 月度趋势 + 最近消费明细。
class ConsumptionPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConsumptionPage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refresh();

private:
    QLabel *m_heroAmount = nullptr;
    QLabel *m_metricMonth = nullptr;
    QLabel *m_metricOrders = nullptr;
    QLabel *m_metricEnergy = nullptr;
    QLabel *m_metricMinutes = nullptr;
    QVBoxLayout *m_monthly = nullptr;
    QVBoxLayout *m_records = nullptr;
    bool m_silentRefresh = false;
};

#endif // CONSUMPTIONPAGE_H
