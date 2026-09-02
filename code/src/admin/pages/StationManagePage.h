#ifndef STATIONMANAGEPAGE_H
#define STATIONMANAGEPAGE_H

#include <QWidget>

// 充电站管理页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. 列表展示: 充电站ID / 站名 / 详细地址 / 经纬度 / 总电桩数 / 当前在线率
//   2. 点击电站行, 查看该站所有电桩的实时状态明细
//   3. 新增电站: 填写站名 / 地址 / 经纬度 / 电桩数量等字段
class StationManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit StationManagePage(QWidget *parent = nullptr);
};

#endif // STATIONMANAGEPAGE_H
