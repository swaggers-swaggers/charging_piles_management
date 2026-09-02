#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QWidget>

class QLabel;
class QTableWidget;

// 电桩状态页: 展示在用/闲置/故障的数量与占比, 反映设备运行健康度
class PileStatusPage : public QWidget
{
    Q_OBJECT

public:
    explicit PileStatusPage(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QLabel *m_summaryLabel;
    QTableWidget *m_table;
};

#endif // PILESTATUSPAGE_H
