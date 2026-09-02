#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include <QWidget>

// 一键导航页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. QWebEngineView 加载腾讯地图 (QT += webenginewidgets)
//   2. 输入起点(当前位置)和终点(目标电站)
//   3. 提供驾车 / 步行等多种出行方式选择
//   4. 点击导航按钮跳转至地图路线规划页面
class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);
};

#endif // NAVIGATIONPAGE_H
