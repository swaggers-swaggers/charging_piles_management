#include "PileStatusPage.h"

#include <QEasingCurve>
#include <QShowEvent>
#include <QStringList>
#include <QVariantAnimation>
#include <QRadialGradient>

#include "PileDao.h"
#include "types.h"

#include <QMap>
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

// ============================================================================
// 设备状态点阵图: 每颗圆点代表一台电桩, 按充电站分组排列,
// 站内桩连续相连、站与站首尾相接(留一格空隙), 标准网格对齐均匀;
// 桩按自身状态着色(闲置/在用/故障), 圆点径向渐变带高光,
// 在线率以大数字 + 胶囊进度条呈现, 点阵入场时渐次点亮。
// ============================================================================
class StatusDotGrid : public QWidget
{
public:
    // byStation: 按充电站分组的桩状态序列(站内桩连续, 站间首尾相接),
    // 状态值 0=闲置 1=在用 2=故障, 与 PileStatus 枚举一致
    StatusDotGrid(const QList<QList<int>> &byStation, QWidget *parent = nullptr)
        : QWidget(parent), m_byStation(byStation)
    {
        m_colors[0] = QColor("#1F9D67");   // 闲置
        m_colors[1] = QColor("#B0863F");   // 在用
        m_colors[2] = QColor("#C5525A");   // 故障
        m_counts[0] = m_counts[1] = m_counts[2] = 0;
        m_total = 0;
        m_numStations = 0;
        for (const QList<int> &station : m_byStation) {
            if (station.isEmpty()) continue;
            ++m_numStations;
            for (int i = 0; i < station.size(); ++i) {
                const int st = station.at(i);
                if (st >= 0 && st < 3) ++m_counts[st];
                ++m_total;
            }
        }
        m_progress = 1.0;
        setObjectName("statusDotGrid");
        setMinimumSize(240, 300);
        m_reveal.setDuration(620);
        m_reveal.setStartValue(0.0);
        m_reveal.setEndValue(1.0);
        m_reveal.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_reveal, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &v) { m_progress = v.toReal(); update(); });
    }

protected:
    void showEvent(QShowEvent *e) override { QWidget::showEvent(e); m_reveal.start(); }
    void hideEvent(QHideEvent *e) override { m_reveal.stop(); QWidget::hideEvent(e); }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width(), h = height();
        const int total = m_total;
        QFont f = p.font();

        // 标题
        f.setPixelSize(14); f.setBold(true); p.setFont(f);
        p.setPen(QColor("#294D3D"));
        p.drawText(QRect(16, 12, w - 32, 24), Qt::AlignCenter, QStringLiteral("设备状态分布"));

        // 在线率大数字(按健康度变色)
        const double rate = total > 0 ? (total - m_counts[2]) * 100.0 / total : 0.0;
        const QColor rateColor = rate >= 90.0 ? QColor("#1F9D67")
                               : rate >= 60.0 ? QColor("#B0863F")
                                              : QColor("#C5525A");
        f.setPixelSize(28); p.setFont(f);
        p.setPen(rateColor);
        p.drawText(QRect(16, 38, w - 32, 40), Qt::AlignCenter,
                   QString("%1%").arg(rate, 0, 'f', 1));
        f.setPixelSize(12); f.setBold(false); p.setFont(f);
        p.setPen(QColor("#74887B"));
        p.drawText(QRect(16, 78, w - 32, 16), Qt::AlignCenter, QStringLiteral("设备在线率"));

        // 在线率胶囊进度条
        {
            const qreal barW = 132, barH = 6;
            const QRectF bar((w - barW) / 2, 98, barW, barH);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#E7EEEA"));
            p.drawRoundedRect(bar, barH / 2, barH / 2);
            if (total > 0) {
                QRectF fill(bar.left(), bar.top(), bar.width() * rate / 100.0, barH);
                p.setBrush(rateColor);
                p.drawRoundedRect(fill, barH / 2, barH / 2);
            }
        }

        // 点阵区: 按充电站排列, 站内桩连续相连, 站与站之间留一格空隙(首尾相接),
        // 标准网格对齐, 圆点径向渐变呈现玻璃珠质感
        const QRect area(24, 114, qMax(0, w - 48), qMax(0, h - 214));
        const qreal step = 12.0;
        const int gapSlots = 1;   // 站间空隙(格)
        const int slotCount = total + qMax(0, m_numStations - 1) * gapSlots;
        int perRow = qMax(1, int(area.width() / step));
        if (slotCount > 0 && (slotCount + perRow - 1) / perRow * step > area.height() + 1) {
            const int maxRows = qMax(1, int(area.height() / step));
            const int maxByWidth = qMax(1, int(area.width() / step));
            perRow = qBound(1, qMin(int(qCeil(slotCount / double(maxRows))), maxByWidth), maxByWidth);
        }
        const qreal dotR = step * 0.38;
        const int visible = int(total * qBound(0.0, m_progress, 1.0));

        p.setPen(Qt::NoPen);
        int slot = 0;
        int stationIdx = 0;
        int inStation = 0;
        int drawn = 0;
        while (drawn < total && drawn < visible) {
            while (stationIdx < m_byStation.size()
                   && inStation >= m_byStation.at(stationIdx).size()) {
                inStation = 0;
                ++stationIdx;
                slot += gapSlots;
            }
            if (stationIdx >= m_byStation.size()) break;
            const int st = m_byStation.at(stationIdx).at(inStation);
            const int r = slot / perRow, c = slot % perRow;
            const qreal x = area.left() + step * c + step / 2;
            const qreal y = area.top() + step * r + step / 2;
            const QColor &col = m_colors[st >= 0 && st < 3 ? st : 1];
            QRadialGradient g(QPointF(x - dotR * 0.45, y - dotR * 0.45), dotR * 2.1);
            g.setColorAt(0.0, col.lighter(135));
            g.setColorAt(0.55, col);
            g.setColorAt(1.0, col.darker(165));
            p.setBrush(g);
            p.drawEllipse(QPointF(x, y), dotR, dotR);
            ++slot;
            ++inStation;
            ++drawn;
        }
        if (total == 0) {
            p.setPen(QColor("#9AA3AF"));
            f.setPixelSize(12); p.setFont(f);
            p.drawText(area, Qt::AlignCenter, QStringLiteral("暂无设备"));
        }

        // 图例: 在用 / 闲置 / 故障
        QStringList names;
        names << QStringLiteral("在用") << QStringLiteral("闲置") << QStringLiteral("故障");
        const int legendIdx[3] = { 1, 0, 2 };   // 在用/闲置/故障 → 颜色与计数下标
        for (int i = 0; i < 3; ++i) {
            const int ci = legendIdx[i];
            const int y = h - 90 + i * 24;
            p.setPen(Qt::NoPen);
            p.setBrush(m_colors[ci]);
            p.drawEllipse(QPointF(30, y + 8), 4, 4);
            p.setPen(QColor("#576D60"));
            p.drawText(QRect(44, y, w - 60, 20), Qt::AlignVCenter,
                       QString("%1   %2 台  ·  %3%").arg(names.at(i)).arg(m_counts[ci])
                           .arg(total ? 100.0 * m_counts[ci] / total : 0.0, 0, 'f', 1));
        }
    }

private:
    QList<QList<int>> m_byStation;   // 按充电站分组的桩状态(0闲置/1在用/2故障)
    int m_counts[3];                 // 闲置 / 在用 / 故障
    int m_total = 0;
    int m_numStations = 0;
    QColor m_colors[3];              // 闲置(绿) / 在用(琥珀) / 故障(红), 构造时赋值
    QVariantAnimation m_reveal;
    qreal m_progress;
};

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

QWidget *PileStatusPage::buildChart(const QList<QList<int>> &byStation)
{
    return new StatusDotGrid(byStation);
}

void PileStatusPage::refreshPage()
{
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

    // 重建点阵图(数据变化后更新): 按充电站分组, 站内桩连续, 站间首尾相接
    QLayoutItem *item = nullptr;
    while ((item = m_chartAreaLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    QList<QList<int>> byStation;
    {
        const QList<PileInfo> piles = PileDao::listAll();
        QMap<int, QList<int>> grouped;   // 按 stationId 分组, 保持站点顺序稳定
        for (const PileInfo &p : piles)
            grouped[p.stationId].append(p.status);
        for (QMap<int, QList<int>>::const_iterator it = grouped.constBegin();
             it != grouped.constEnd(); ++it)
            byStation.append(it.value());
    }
    m_chartAreaLayout->addWidget(buildChart(byStation));

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
