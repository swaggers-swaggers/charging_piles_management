#include "StationManagePage.h"

#include <QLabel>
#include <QVBoxLayout>

StationManagePage::StationManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("充电站管理", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中...\n\n"
                  "计划实现:\n"
                  "  1. 充电站列表: ID / 站名 / 地址 / 经纬度 / 总电桩数 / 在线率\n"
                  "  2. 点击电站行查看站内电桩实时状态明细\n"
                  "  3. 新增电站 (填写站名 / 地址 / 经纬度 / 电桩数量)");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
