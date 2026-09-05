#ifndef CHARGECHARTWIDGET_H
#define CHARGECHARTWIDGET_H

#include <QPointF>
#include <QVector>
#include <QWidget>

// 充电实时曲线: 自绘折线图, 展示电量(度)或金额(元)随充电时长(分钟)的变化.
// 数据由 ChargingPage 从 PushOrderProgress 推送中喂入, 支持电量/金额双模式切换.
class ChargeChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChargeChartWidget(QWidget *parent = nullptr);

    void addPoint(int minutes, double energy, double amount);
    void clearData();
    void setMode(int mode);   // 0=电量 1=金额
    int mode() const { return m_mode; }

    QSize minimumSizeHint() const override { return QSize(200, 140); }
    QSize sizeHint() const override { return QSize(400, 160); }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    struct Point { int minutes; double energy; double amount; };
    QVector<Point> m_data;
    int m_mode = 0;   // 0=电量 1=金额
};

#endif // CHARGECHARTWIDGET_H
