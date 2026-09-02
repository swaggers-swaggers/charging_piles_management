#ifndef PILEMANAGEPAGE_H
#define PILEMANAGEPAGE_H

#include <QWidget>

// 充电桩管理页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. 列表展示: 电桩编号 / 所属电站 / 类型(快充慢充) / 功率(kW) / 当前状态 /
//      累计充电次数 / 累计充电时长
//   2. 选中电桩后执行 "远程重启" (模拟向电桩发送重启指令)
class PileManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit PileManagePage(QWidget *parent = nullptr);
};

#endif // PILEMANAGEPAGE_H
