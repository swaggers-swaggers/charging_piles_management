#include "NearbyStationsPage.h"

#include <QLabel>
#include <QVBoxLayout>

NearbyStationsPage::NearbyStationsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("附近充电站", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中...\n\n"
                  "计划实现:\n"
                  "  1. 模拟GPS定位 (下拉选择区域 / 手动输入地址)\n"
                  "  2. 腾讯地图 Web API 地址转经纬度\n"
                  "  3. 按距离由近及远展示充电站卡片\n"
                  "     (站名 / 价格 / 电桩总数 / 空闲数量 / 距离)\n"
                  "  4. 点击充电站查看站内电桩详细信息");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
