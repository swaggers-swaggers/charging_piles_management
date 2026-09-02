#include "PileStatusPage.h"

#include <QLabel>
#include <QVBoxLayout>

PileStatusPage::PileStatusPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("电桩状态", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中...\n\n"
                  "计划实现:\n"
                  "  1. 表格展示所有电桩状态分布 (在用 / 闲置 / 故障)\n"
                  "  2. 各状态数量及占比百分比\n"
                  "  3. 设备整体运行健康度概览");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
