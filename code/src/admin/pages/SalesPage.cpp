#include "SalesPage.h"

#include <QLabel>
#include <QVBoxLayout>

SalesPage::SalesPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("销售业绩", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中...\n\n"
                  "计划实现:\n"
                  "  1. 今日营收 / 本月营收 / 总营收 三大指标卡\n"
                  "  2. 近7日 / 近30日 营收趋势折线图 (QChart)\n"
                  "  3. 时间维度切换查看营收变化趋势");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
