#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QList>
#include <QWidget>

class QLabel;
class QTableWidget;
class QVBoxLayout;

// 电桩状态页: 统计卡片(在用/闲置/故障/在线率) + 设备状态点阵图 + 明细表格, 反映设备运行健康度
class PileStatusPage : public QWidget
{
    Q_OBJECT

public:
    explicit PileStatusPage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

private slots:
    void refresh();

private:
    // 按充电站分组重建点阵图(站内桩连续/站间首尾相接), 返回可加入布局的控件
    QWidget *buildChart(const QList<QList<int>> &byStation);

    QLabel *m_inUseValue;
    QLabel *m_idleValue;
    QLabel *m_faultValue;
    QLabel *m_rateValue;
    QLabel *m_summaryLabel;
    QVBoxLayout *m_chartAreaLayout;
    QTableWidget *m_table;
};

#endif // PILESTATUSPAGE_H
