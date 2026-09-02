#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include <QWidget>

// 一键导航页 (阶段 2 实现, 模拟导航, 不依赖外网)
// 计划功能(项目说明书 1.4):
//   1. 输入起点(当前位置)和终点(目标电站)
//   2. 提供驾车 / 步行等多种出行方式选择
//   3. 自绘模拟地图: 站点散点 + 当前位置 + 路线折线 + 预计时长/距离
class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);
};

#endif // NAVIGATIONPAGE_H
