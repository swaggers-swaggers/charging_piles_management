#ifndef SALESPAGE_H
#define SALESPAGE_H

#include <QWidget>

// 销售业绩页 (框架占位, 待实现)
// 计划功能(项目说明书 1.4):
//   1. 今日营收 / 本月营收 / 总营收 三大核心数字指标
//   2. 近7日 / 近30日 营收趋势折线图 (QT += charts, QChart)
//   3. 时间维度切换查看营收变化趋势
class SalesPage : public QWidget
{
    Q_OBJECT

public:
    explicit SalesPage(QWidget *parent = nullptr);
};

#endif // SALESPAGE_H
