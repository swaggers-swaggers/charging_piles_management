#include "ChargingPage.h"

#include <QLabel>
#include <QVBoxLayout>

ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("电动汽车充电", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中(阶段2)...\n\n"
                  "计划实现:\n"
                  "  1. 充电前检查未完成订单并强制结算\n"
                  "  2. 选择空闲电桩开始充电\n"
                  "  3. 完整的 \"预约 — 充电 — 计费 — 结算\" 流程");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
