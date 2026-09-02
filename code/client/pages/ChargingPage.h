#ifndef CHARGINGPAGE_H
#define CHARGINGPAGE_H

#include <QWidget>

// 电动汽车充电页 (阶段 2 实现, 数据经 Socket 由服务端提供)
// 计划功能(项目说明书 1.4):
//   1. 充电前检查: 查询当前用户是否存在"充电中"未完成订单,
//      有则弹窗提示"您有未完成的充电订单, 请先结算"并强制跳转结算
//   2. 选择空闲电桩开始充电(服务端生成订单, 桩置"在用")
//   3. 完整的 "预约 — 充电 — 计费 — 结算" 流程,
//      服务端定时推送充电进度(电量/金额)
class ChargingPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChargingPage(QWidget *parent = nullptr);
};

#endif // CHARGINGPAGE_H
