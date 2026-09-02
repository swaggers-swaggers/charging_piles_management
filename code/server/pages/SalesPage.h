#ifndef SALESPAGE_H
#define SALESPAGE_H

#include <QWidget>

class QComboBox;
class QLabel;
class QHBoxLayout;

// 销售业绩页: 今日/本月/总营收三大指标 + 近7日/近30日营收趋势(折线图+柱状图并排)
// 图表实现: 安装了 Qt Charts 用 QChart(HAVE_QTCHARTS), 未安装自动降级为自绘
class SalesPage : public QWidget
{
    Q_OBJECT

public:
    explicit SalesPage(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QLabel *m_todayVal;
    QLabel *m_monthVal;
    QLabel *m_totalVal;
    QComboBox *m_rangeCombo;
    QHBoxLayout *m_chartLayout;
};

#endif // SALESPAGE_H
