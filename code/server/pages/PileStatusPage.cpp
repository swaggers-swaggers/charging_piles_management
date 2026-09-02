#include "PileStatusPage.h"

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

#ifdef HAVE_QTCHARTS
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif
#endif

namespace {
QString percentText(int part, int total)
{
    if (total <= 0)
        return "0%";
    return QString::number(part * 100.0 / total, 'f', 1) + "%";
}

// 无 Qt Charts 时的降级方案: 用 QPainter 自绘环形占比图
class PlainPieChart : public QWidget
{
public:
    PlainPieChart(int inUse, int idle, int fault, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_inUse(inUse)
        , m_idle(idle)
        , m_fault(fault)
    {
        setMinimumSize(280, 200);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int total = m_inUse + m_idle + m_fault;
        const QRectF ring = QRectF(rect()).adjusted(24, 24, -150, -24);

        // 画环形扇区
        if (total > 0) {
            struct Seg { int v; QColor c; };
            const Seg segs[] = {
                { m_inUse, QColor("#2F80ED") },
                { m_idle,  QColor("#1D976C") },
                { m_fault, QColor("#E5484D") },
            };
            // drawPie 角度单位 = 1/16 度, 0 在 3 点方向, 顺时针
            qreal start = 90.0 * 16.0;
            for (const Seg &s : segs) {
                if (s.v <= 0)
                    continue;
                const qreal span = s.v * 360.0 * 16.0 / total;
                p.setBrush(s.c);
                p.setPen(Qt::NoPen);
                p.drawPie(ring, int(start), int(-span));
                start -= span;
            }
            // 挖空中心成环形
            p.setBrush(palette().color(QPalette::Base));
            p.drawEllipse(ring.adjusted(ring.width() * 0.28, ring.height() * 0.28,
                                        -ring.width() * 0.28, -ring.height() * 0.28));
            // 中心显示总数
            p.setPen(QColor("#1F2A3C"));
            QFont f = p.font();
            f.setPointSize(16);
            f.setBold(true);
            p.setFont(f);
            p.drawText(ring, Qt::AlignCenter, QString::number(total));
        } else {
            p.setPen(QColor("#94A3B8"));
            p.drawText(ring, Qt::AlignCenter, "暂无数据");
        }

        // 右侧图例
        struct Legend { const char *name; int v; QColor c; };
        const Legend legs[] = {
            { "在用", m_inUse, QColor("#2F80ED") },
            { "闲置", m_idle,  QColor("#1D976C") },
            { "故障", m_fault, QColor("#E5484D") },
        };
        int y = 30;
        for (const Legend &l : legs) {
            p.setPen(Qt::NoPen);
            p.setBrush(l.c);
            p.drawRoundedRect(QRect(rect().width() - 130, y, 14, 14), 3, 3);
            p.setPen(QColor("#475569"));
            QFont f = p.font();
            f.setPointSize(10);
            p.setFont(f);
            p.drawText(QRect(rect().width() - 110, y - 2, 100, 20),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString("%1 %2 台 (%3)")
                           .arg(QString::fromUtf8(l.name)).arg(l.v)
                           .arg(percentText(l.v, total)));
            y += 26;
        }
    }

private:
    int m_inUse;
    int m_idle;
    int m_fault;
};
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
        capLabel->setStyleSheet("color:#64748B; font-size:13px; background:transparent;");
        auto *value = new QLabel("-", card);
        value->setStyleSheet(QString("color:%1; font-size:24px; font-weight:bold; background:transparent;")
                                 .arg(color.name()));
        v->addWidget(capLabel);
        v->addWidget(value);
        cards->addWidget(card, 1);
        *valueOut = value;
    };
    makeCard("在用 (充电中)", QColor("#2F80ED"), &m_inUseValue);
    makeCard("闲置 (可用)",   QColor("#1D976C"), &m_idleValue);
    makeCard("故障 (需处理)", QColor("#E5484D"), &m_faultValue);
    makeCard("在线率",        QColor("#F2994A"), &m_rateValue);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    m_summaryLabel->setStyleSheet(
        "background:#FFFFFF; border:1px solid #E2E8F0; border-radius:10px;"
        "padding:10px; color:#475569; font-size:13px;");

    // 图表 + 明细表 并排
    QWidget *chartArea = new QWidget(this);
    m_chartAreaLayout = new QVBoxLayout(chartArea);
    m_chartAreaLayout->setContentsMargins(0, 0, 0, 0);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(4);
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
#ifdef HAVE_QTCHARTS
    QPieSeries *series = new QPieSeries();
    QPieSlice *s1 = series->append("在用", qMax(inUse, 0));
    QPieSlice *s2 = series->append("闲置", qMax(idle, 0));
    QPieSlice *s3 = series->append("故障", qMax(fault, 0));
    s1->setColor(QColor("#2F80ED"));
    s2->setColor(QColor("#1D976C"));
    s3->setColor(QColor("#E5484D"));
    for (QPieSlice *s : series->slices())
        s->setLabelVisible(false);
    series->setHoleSize(0.45);
    series->setPieSize(0.82);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor("#475569"));
    chart->setBackgroundVisible(false);
    chart->setTitle("电桩状态占比");
    chart->setTitleBrush(QColor("#1F2A3C"));

    auto *view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    return view;
#else
    return new PlainPieChart(inUse, idle, fault);
#endif
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
