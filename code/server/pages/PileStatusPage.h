#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QWidget>

class QLabel;
class QTableWidget;
class QVBoxLayout;

// 电桩状态页: 统计卡片(在用/闲置/故障/在线率) + 环形占比图 + 明细表格, 反映设备运行健康度
class PileStatusPage : public QWidget
{
    Q_OBJECT

public:
    explicit PileStatusPage(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    // 重建环形占比图(装了 Qt Charts 用 QChart, 否则自绘), 返回可加入布局的控件
    QWidget *buildChart(int inUse, int idle, int fault);

    QLabel *m_inUseValue;
    QLabel *m_idleValue;
    QLabel *m_faultValue;
    QLabel *m_rateValue;
    QLabel *m_summaryLabel;
    QVBoxLayout *m_chartAreaLayout;
    QTableWidget *m_table;
};

#endif // PILESTATUSPAGE_H
