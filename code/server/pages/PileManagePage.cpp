#include "PileManagePage.h"

#include "LogDao.h"
#include "PileDao.h"
#include "ServerSession.h"

#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString pileStatusText(int status)
{
    switch (status) {
    case PileIdle:  return "闲置";
    case PileInUse: return "在用";
    case PileFault: return "故障";
    }
    return "未知";
}

QColor pileStatusColor(int status)
{
    switch (status) {
    case PileInUse: return QColor("#B0863F");
    case PileFault: return QColor("#C5525A");
    default:        return QColor("#1F9D67");
    }
}
} // namespace

PileManagePage::PileManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("充电桩管理", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *refreshBtn = new QPushButton("刷新", this);
    refreshBtn->setObjectName("searchButton");
    m_restartBtn = new QPushButton("远程重启", this);
    m_restartBtn->setObjectName("restartButton");
    m_restartBtn->setEnabled(false);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(m_restartBtn);
    btnRow->addStretch();

    m_table = new QTableWidget(this);
    m_table->setObjectName("pileManageTable");
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels(
        { "电桩编号", "所属电站", "类型", "功率(kW)", "当前状态", "累计充电次数", "累计时长(小时)" });

    layout->addWidget(title);
    layout->addLayout(btnRow);
    layout->addWidget(m_table, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &PileManagePage::refresh);
    connect(m_restartBtn, &QPushButton::clicked, this, &PileManagePage::onRestartClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &PileManagePage::onSelectionChanged);
    refresh();
}

void PileManagePage::refresh()
{
    const QList<PileInfo> piles = PileDao::listAll();
    m_table->setRowCount(piles.size());
    for (int i = 0; i < piles.size(); ++i) {
        const PileInfo &p = piles[i];
        auto *codeItem = new QTableWidgetItem(p.code);
        codeItem->setData(Qt::UserRole, p.id);          // 记录电桩id
        codeItem->setData(Qt::UserRole + 1, p.status);  // 记录状态
        m_table->setItem(i, 0, codeItem);
        m_table->setItem(i, 1, new QTableWidgetItem(p.stationName));
        m_table->setItem(i, 2, new QTableWidgetItem(p.type == PileFast ? "快充" : "慢充"));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(p.power, 'f', 1)));
        auto *statusItem = new QTableWidgetItem(pileStatusText(p.status));
        statusItem->setForeground(QBrush(pileStatusColor(p.status)));
        m_table->setItem(i, 4, statusItem);
        m_table->setItem(i, 5, new QTableWidgetItem(QString::number(p.totalCount)));
        m_table->setItem(i, 6,
                         new QTableWidgetItem(QString::number(p.totalDuration / 60.0, 'f', 1)));
    }
    m_table->resizeColumnsToContents();
    onSelectionChanged();
}

void PileManagePage::onSelectionChanged()
{
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        m_selectedId = -1;
        m_restartBtn->setEnabled(false);
        return;
    }
    const QTableWidgetItem *codeItem = m_table->item(selected.first()->row(), 0);
    m_selectedId = codeItem->data(Qt::UserRole).toInt();
    m_selectedCode = codeItem->text();
    m_selectedStatus = codeItem->data(Qt::UserRole + 1).toInt();
    m_restartBtn->setEnabled(m_selectedStatus != PileInUse);
}

void PileManagePage::onRestartClicked()
{
    if (m_selectedId < 0)
        return;
    if (m_selectedStatus == PileInUse) {
        QMessageBox::warning(this, "提示", "电桩正在使用中, 暂不能重启");
        return;
    }
    if (QMessageBox::question(this, "远程重启",
                              QString("确定向电桩 %1 发送重启指令吗?").arg(m_selectedCode))
        != QMessageBox::Yes)
        return;

    QString errMsg;
    if (!PileDao::restart(m_selectedId, &errMsg)) {
        QMessageBox::warning(this, "远程重启失败", errMsg);
        return;
    }

    LogDao::record(ServerSession::instance().adminName, "远程重启",
                   QString("电桩 %1 重启成功, 状态恢复闲置").arg(m_selectedCode));
    QMessageBox::information(this, "提示",
                             QString("重启指令已执行, 电桩 %1 状态恢复闲置").arg(m_selectedCode));
    refresh();
}
