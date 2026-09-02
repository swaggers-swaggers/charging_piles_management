#ifndef NEARBYSTATIONSPAGE_H
#define NEARBYSTATIONSPAGE_H

#include <QWidget>

// 附近充电站查询页 (阶段 2 实现, 数据经 Socket 由服务端提供)
// 计划功能(项目说明书 1.4):
//   1. 模拟GPS定位: 下拉选择区域或手动输入地址(映射固定坐标)
//   2. 服务端计算各站与当前位置的距离并按近→远排序
//   3. 站卡片展示: 站名 / 充电价格(元/度) / 电桩总数与空闲数量 / 距离(公里)
//   4. 点击充电站查看该站所有电桩详细信息(编号 / 类型 / 状态 / 功率)
class NearbyStationsPage : public QWidget
{
    Q_OBJECT

public:
    explicit NearbyStationsPage(QWidget *parent = nullptr);
};

#endif // NEARBYSTATIONSPAGE_H
