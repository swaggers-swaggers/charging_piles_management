#include "PileStatusPage.h"

#include "PileDao.h"

#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

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
    topRow->addWidget(refreshBtn);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    QFont summaryFont = m_summaryLabel->font();
    summaryFont.setPointSize(14);
    summaryFont.setBold(true);
    m_summaryLabel->setFont(summaryFont);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({ "状态", "数量(台)", "占比", "说明" });

    layout->addWidget(title);
    layout->addLayout(topRow);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_table, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &PileStatusPage::refresh);
    refresh();
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

    m_summaryLabel->setText(QString("设备运行健康度:  总计 %1 台   |   在用 %2   闲置 %3   故障 %4")
                                .arg(total).arg(inUse).arg(idle).arg(fault));

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
