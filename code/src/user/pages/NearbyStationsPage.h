#ifndef NEARBYSTATIONSPAGE_H
#define NEARBYSTATIONSPAGE_H

#include <QWidget>

// 附近充电站查询页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. 模拟GPS定位: 下拉选择区域或手动输入地址
//   2. 调用腾讯地图 Web API 将地址转换为经纬度坐标
//   3. 按距离由近及远展示充电站列表(卡片):
//      站名 / 充电价格(元/度) / 电桩总数与空闲数量 / 距离(公里)
//   4. 点击充电站查看该站所有电桩详细信息(编号 / 类型 / 状态 / 功率)
class NearbyStationsPage : public QWidget
{
    Q_OBJECT

public:
    explicit NearbyStationsPage(QWidget *parent = nullptr);
};

#endif // NEARBYSTATIONSPAGE_H
