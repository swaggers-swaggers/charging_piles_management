#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QWidget>

// 电桩状态页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. 表格展示所有电桩的状态分布: 在用 / 闲置 / 故障
//   2. 显示各状态的数量及占比百分比
//   3. 直观反映设备整体运行健康度
class PileStatusPage : public QWidget
{
    Q_OBJECT

public:
    explicit PileStatusPage(QWidget *parent = nullptr);
};

#endif // PILESTATUSPAGE_H
