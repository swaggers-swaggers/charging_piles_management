#include "NavigationPage.h"

#include <QLabel>
#include <QVBoxLayout>

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("一键导航", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中(阶段2)...\n\n"
                  "计划实现:\n"
                  "  1. 输入起点(当前位置)和终点(目标电站)\n"
                  "  2. 驾车 / 步行等多种出行方式选择\n"
                  "  3. 自绘模拟地图: 路线 + 预计时长/距离");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
