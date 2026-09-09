#ifndef CHARGECHARTWIDGET_H
#define CHARGECHARTWIDGET_H

#include <QPointF>
#include <QVector>
#include <QWidget>

// 充电实时曲线: 自绘折线图, 展示功率(kW)、累计电量(度)与金额(元)。
// 数据由 ChargingPage 从 PushOrderProgress 推送中喂入, 支持功率/电量/金额切换，功率缺失时不伪造样本。
class ChargeChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChargeChartWidget(QWidget *parent = nullptr);

    // 每个订单拥有独立的数据序列；切换订单时自动清空上一条曲线。
    void beginSession(int orderId);
    void addPoint(int minutes, double energy, double amount, double power = -1);
    void clearData();
    void setMode(int mode);   // 0=电量 1=金额 2=功率
    int mode() const { return m_mode; }
    int sessionId() const { return m_sessionId; }
    int pointCount() const { return m_data.size(); }

    QSize minimumSizeHint() const override { return QSize(200, 140); }
    QSize sizeHint() const override { return QSize(400, 160); }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    struct Point { int minutes; double energy; double amount; double power; };
    QVector<Point> m_data;
    int m_mode = 2;   // 0=电量 1=金额 2=功率
    int m_sessionId = -1;
};

#endif // CHARGECHARTWIDGET_H
