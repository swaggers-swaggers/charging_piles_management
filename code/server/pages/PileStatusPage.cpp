#include "PileStatusPage.h"
#include "DonutChart.h"

#include "PileDao.h"

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {
QString percentText(int part, int total)
{
    if (total <= 0)
        return "0%";
    return QString::number(part * 100.0 / total, 'f', 1) + "%";
}

} // namespace

PileStatusPage::PileStatusPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("电桩状态", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->addStretch();
    QPushButton *refreshBtn = new QPushButton("刷新", this);
    refreshBtn->setObjectName("refreshButton");
    topRow->addWidget(refreshBtn);

    // 统计卡片: 在用 / 闲置 / 故障 / 在线率
    QHBoxLayout *cards = new QHBoxLayout();
    cards->setSpacing(14);

    auto makeCard = [this, cards](const QString &cap, QColor color, QLabel **valueOut) {
        auto *card = new QFrame(this);
        card->setObjectName("statCard");
        card->setFixedHeight(96);
        auto *v = new QVBoxLayout(card);
        v->setContentsMargins(18, 12, 18, 12);
        v->setSpacing(4);
        auto *capLabel = new QLabel(cap, card);
        capLabel->setObjectName("metricTitle");
        auto *value = new QLabel("-", card);
        value->setObjectName("metricValue");
        value->setStyleSheet(QString("color:%1;").arg(color.name()));
        v->addWidget(capLabel);
        v->addWidget(value);
        cards->addWidget(card, 1);
        *valueOut = value;
    };
    makeCard("在用 (充电中)", QColor("#B0863F"), &m_inUseValue);
    makeCard("闲置 (可用)",   QColor("#1F9D67"), &m_idleValue);
    makeCard("故障 (需处理)", QColor("#C5525A"), &m_faultValue);
    makeCard("在线率",        QColor("#B0863F"), &m_rateValue);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("summaryLabel");
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setAlignment(Qt::AlignCenter);

    // 图表 + 明细表 并排
    QWidget *chartArea = new QWidget(this);
    chartArea->setObjectName("chartArea");
    chartArea->setAttribute(Qt::WA_StyledBackground);
    m_chartAreaLayout = new QVBoxLayout(chartArea);
    m_chartAreaLayout->setContentsMargins(0, 0, 0, 0);

    m_table = new QTableWidget(this);
    m_table->setObjectName("statusTable");
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(4);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setMinimumSectionSize(80);
    m_table->setHorizontalHeaderLabels({ "状态", "数量(台)", "占比", "说明" });

    QHBoxLayout *chartsRow = new QHBoxLayout();
    chartsRow->setSpacing(14);
    chartsRow->addWidget(chartArea, 1);
    chartsRow->addWidget(m_table, 1);

    layout->addWidget(title);
    layout->addLayout(topRow);
    layout->addLayout(cards);
    layout->addLayout(chartsRow, 1);
    layout->addWidget(m_summaryLabel);

    connect(refreshBtn, &QPushButton::clicked, this, &PileStatusPage::refresh);
    refresh();
}

QWidget *PileStatusPage::buildChart(int inUse, int idle, int fault)
{
    return new DonutChart(inUse, idle, fault);
}

void PileStatusPage::refresh()
{
    int idle = 0, inUse = 0, fault = 0;
    QString errMsg;
    if (!PileDao::statusCounts(&idle, &inUse, &fault, &errMsg)) {
        m_summaryLabel->setText(errMsg);
        return;
    }
    const int total = idle + inUse + fault;
    const double rate = (total > 0) ? (total - fault) * 100.0 / total : 0.0;

    m_inUseValue->setText(QString("%1 台").arg(inUse));
    m_idleValue->setText(QString("%1 台").arg(idle));
    m_faultValue->setText(QString("%1 台").arg(fault));
    m_rateValue->setText(QString("%1%").arg(rate, 0, 'f', 1));

    m_summaryLabel->setText(QString("设备运行健康度:  总计 %1 台   |   在用 %2 台 (%3)   闲置 %4 台 (%5)   故障 %6 台 (%7)   在线率 %8%")
                                .arg(total)
                                .arg(inUse).arg(percentText(inUse, total))
                                .arg(idle).arg(percentText(idle, total))
                                .arg(fault).arg(percentText(fault, total))
                                .arg(rate, 0, 'f', 1));

    // 重建环形占比图(数据变化后更新)
    QLayoutItem *item = nullptr;
    while ((item = m_chartAreaLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_chartAreaLayout->addWidget(buildChart(inUse, idle, fault));

    struct Row { const char *name; int count; const char *desc; };
    const Row rows[] = {
        { "在用", inUse,  "正在充电的桩" },
        { "闲置", idle,   "空闲可用的桩" },
        { "故障", fault,  "需要检修/远程重启处理" },
    };

    m_table->setRowCount(4);
    for (int i = 0; i < 3; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(rows[i].name)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(rows[i].count)));
        m_table->setItem(i, 2, new QTableWidgetItem(percentText(rows[i].count, total)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::fromUtf8(rows[i].desc)));
    }
    m_table->setItem(3, 0, new QTableWidgetItem("合计"));
    m_table->setItem(3, 1, new QTableWidgetItem(QString::number(total)));
    m_table->setItem(3, 2, new QTableWidgetItem("100%"));
    m_table->setItem(3, 3, new QTableWidgetItem("全部电桩"));

    m_table->resizeColumnsToContents();
}
