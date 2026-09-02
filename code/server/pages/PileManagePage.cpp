#include "PileManagePage.h"

#include <QLabel>
#include <QVBoxLayout>

PileManagePage::PileManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("充电桩管理", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中(阶段1)...\n\n"
                  "计划实现:\n"
                  "  1. 电桩列表: 编号 / 所属电站 / 类型 / 功率 / 状态 /\n"
                  "     累计充电次数 / 累计充电时长\n"
                  "  2. 选中电桩后执行 \"远程重启\" (模拟发送重启指令)");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
