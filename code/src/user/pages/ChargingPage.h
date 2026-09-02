#ifndef CHARGINGPAGE_H
#define CHARGINGPAGE_H

#include <QWidget>

// 电动汽车充电页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. 充电前检查: 查询当前用户是否存在"充电中"未完成订单,
//      有则弹窗提示"您有未完成的充电订单, 请先结算"并强制跳转结算页面
//   2. 选择空闲电桩开始充电
//   3. 完整的 "预约 — 充电 — 计费 — 结算" 流程
class ChargingPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChargingPage(QWidget *parent = nullptr);
};

#endif // CHARGINGPAGE_H
