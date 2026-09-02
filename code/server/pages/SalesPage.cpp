#include "SalesPage.h"

#include "DatabaseManager.h"
#include "OrderDao.h"

#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtGlobal>

#ifdef HAVE_QTCHARTS
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QDateTimeAxis>
#include <QLineSeries>
#include <QValueAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE   // Qt5 的图表类位于 QtCharts 命名空间, Qt6 已移除该命名空间
#endif
#endif

namespace {
// 指标卡片: 白底圆角框 + 标题 + 大数字
QFrame *createMetricCard(const QString &title, QLabel **valueLabel, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setObjectName("metricCard");
    card->setMinimumHeight(90);
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 14, 20, 14);

    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("metricTitle");
    QLabel *valLabel = new QLabel("0.00 元", card);
    valLabel->setObjectName("metricValue");
    valLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(titleLabel);
    layout->addWidget(valLabel, 1);
    *valueLabel = valLabel;
    return card;
}
} // namespace

#ifndef HAVE_QTCHARTS
// 降级方案(计划书风险应对): 未安装 Qt Charts 时, 用 QPainter 自绘简单折线图
class PlainLineChart : public QWidget
{
public:
    explicit PlainLineChart(QWidget *parent = nullptr) : QWidget(parent) {}

    void setData(const QVector<QPair<QString, double>> &data)
    {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        const QRectF plot = QRectF(rect()).adjusted(52, 12, -14, -30);
        const int n = m_data.size();
        if (n == 0)
            return;

        double minV = 0, maxV = 0;
        for (const auto &d : m_data) {
            minV = qMin(minV, d.second);
            maxV = qMax(maxV, d.second);
        }
        if (maxV == minV)
            maxV += 1.0;
        const double range = maxV - minV;
        const int step = n > 1 ? n - 1 : 1;

        // 网格 + Y 轴刻度
        QPen gridPen(QColor("#E2E8F0"));
        gridPen.setWidth(1);
        p.setPen(gridPen);
        for (int i = 0; i <= 4; ++i) {
            const double y = plot.top() + plot.height() * i / 4.0;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            const double val = maxV - range * i / 4.0;
            p.setPen(QColor("#64748B"));
            p.drawText(QRectF(0, y - 10, plot.left() - 8, 20),
                       Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 0));
            p.setPen(gridPen);
        }

        // X 轴标签
        p.setPen(QColor("#64748B"));
        for (int i = 0; i < n; ++i) {
            const double x = plot.left() + plot.width() * i / step;
            p.drawText(QRectF(x - 32, plot.bottom() + 6, 64, 20),
                       Qt::AlignHCenter | Qt::AlignTop, m_data[i].first);
        }

        // 折线 + 数据点
        QPainterPath path;
        for (int i = 0; i < n; ++i) {
            const double x = plot.left() + plot.width() * i / step;
            const double y = plot.bottom() - plot.height() * (m_data[i].second - minV) / range;
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        QPen linePen(QColor("#2F80ED"));
        linePen.setWidth(2);
        p.setPen(linePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        p.setBrush(QColor("#2F80ED"));
        for (int i = 0; i < n; ++i) {
            const double x = plot.left() + plot.width() * i / step;
            const double y = plot.bottom() - plot.height() * (m_data[i].second - minV) / range;
            p.drawEllipse(QPointF(x, y), 3, 3);
        }
    }

private:
    QVector<QPair<QString, double>> m_data;
};

// 降级方案: 未安装 Qt Charts 时, 用 QPainter 自绘柱状图
class PlainBarChart : public QWidget
{
public:
    explicit PlainBarChart(QWidget *parent = nullptr) : QWidget(parent) {}

    void setData(const QVector<QPair<QString, double>> &data)
    {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        const QRectF plot = QRectF(rect()).adjusted(52, 12, -14, -30);
        const int n = m_data.size();
        if (n == 0)
            return;

        double maxV = 0;
        for (const auto &d : m_data)
            maxV = qMax(maxV, d.second);
        if (maxV <= 0)
            maxV = 1.0;

        // 网格 + Y 轴刻度
        QPen gridPen(QColor("#E2E8F0"));
        gridPen.setWidth(1);
        p.setPen(gridPen);
        for (int i = 0; i <= 4; ++i) {
            const double y = plot.top() + plot.height() * i / 4.0;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            const double val = maxV * (4 - i) / 4.0;
            p.setPen(QColor("#64748B"));
            p.drawText(QRectF(0, y - 10, plot.left() - 8, 20),
                       Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 0));
            p.setPen(gridPen);
        }

        // X 轴标签
        p.setPen(QColor("#64748B"));
        for (int i = 0; i < n; ++i) {
            const double x = plot.left() + plot.width() * (i + 0.5) / n;
            p.drawText(QRectF(x - 32, plot.bottom() + 6, 64, 20),
                       Qt::AlignHCenter | Qt::AlignTop, m_data[i].first);
        }

        // 柱体
        const double barW = qMin(plot.width() * 0.6 / n, 40.0);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < n; ++i) {
            const double h = plot.height() * (m_data[i].second / maxV);
            const double x = plot.left() + plot.width() * (i + 0.5) / n - barW / 2;
            const QRectF bar(x, plot.bottom() - h, barW, h);
            p.setBrush(QColor("#FFB020"));
            p.drawRoundedRect(bar, 2, 2);
        }
    }

private:
    QVector<QPair<QString, double>> m_data;
};
#endif // !HAVE_QTCHARTS

SalesPage::SalesPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("销售业绩", this);
    title->setObjectName("pageTitle");

    // 指标卡
    QHBoxLayout *metricRow = new QHBoxLayout();
    metricRow->addWidget(createMetricCard("今日营收 (元)", &m_todayVal, this));
    metricRow->addWidget(createMetricCard("本月营收 (元)", &m_monthVal, this));
    metricRow->addWidget(createMetricCard("总营收 (元)", &m_totalVal, this));
    metricRow->setSpacing(16);

    // 时间维度切换 + 图表区
    QHBoxLayout *chartHead = new QHBoxLayout();
    chartHead->addStretch();
    QPushButton *demoBtn = new QPushButton("生成演示数据", this);
    demoBtn->setCursor(Qt::PointingHandCursor);
    demoBtn->setToolTip("自动生成近30天固定演示订单(会重建演示用户的订单, 供图表/大屏展示)");
    chartHead->addWidget(demoBtn);
    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem("近7日");
    m_rangeCombo->addItem("近30日");
    chartHead->addWidget(m_rangeCombo);

    connect(demoBtn, &QPushButton::clicked, this, [this]() {
        QString err;
        DatabaseManager::instance().generateDemoData(&err);
        if (!err.isEmpty()) {
            QMessageBox::warning(this, "生成失败", err);
            return;
        }
        QMessageBox::information(this, "提示",
                                 "已生成近30天固定演示订单数据, 图表已刷新\n"
                                 "(服务端将自动导出到 Web 大屏 data.json)");
        refresh();
    });

    QWidget *chartHost = new QWidget(this);
    m_chartLayout = new QHBoxLayout(chartHost);
    m_chartLayout->setContentsMargins(0, 0, 0, 0);
    m_chartLayout->setSpacing(14);

    layout->addWidget(title);
    layout->addLayout(metricRow);
    layout->addLayout(chartHead);
    layout->addWidget(chartHost, 1);

    connect(m_rangeCombo, &QComboBox::currentIndexChanged, this, &SalesPage::refresh);
    refresh();
}

void SalesPage::refresh()
{
    // ---- 指标卡 ----
    double today = 0.0, month = 0.0, total = 0.0;
    QString errMsg;
    if (!OrderDao::salesSummary(&today, &month, &total, &errMsg)) {
        m_todayVal->setText("统计失败");
        m_monthVal->setText(errMsg);
        m_totalVal->setText("");
    } else {
        m_todayVal->setText(QString::number(today, 'f', 2));
        m_monthVal->setText(QString::number(month, 'f', 2));
        m_totalVal->setText(QString::number(total, 'f', 2));
    }

    // ---- 折线图(每次重建, 规避跨Qt版本的图表成员类型差异) ----
    QLayoutItem *item = nullptr;
    while ((item = m_chartLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const int days = (m_rangeCombo->currentIndex() == 1) ? 30 : 7;
    const QVector<QPair<QString, double>> daily = OrderDao::dailyRevenue(days);

    double maxY = 10.0;
    for (const auto &entry : daily)
        maxY = qMax(maxY, entry.second);

#ifdef HAVE_QTCHARTS
    // ---- 左: 折线图 ----
    QLineSeries *series = new QLineSeries();
    for (const auto &entry : daily) {
        const QDateTime dt = QDateTime(QDate::fromString(entry.first, "yyyy-MM-dd"), QTime(12, 0));
        series->append(dt.toMSecsSinceEpoch(), entry.second);
    }

    QChart *chart = new QChart();
    chart->setTitle(QString("营收趋势 (近%1日, 单位: 元)").arg(days));
    chart->legend()->hide();

    QPen pen(QColor("#2F80ED"));
    pen.setWidth(2);
    series->setPen(pen);
    chart->addSeries(series);

    QDateTimeAxis *axisX = new QDateTimeAxis(chart);
    axisX->setFormat("MM-dd");
    axisX->setTickCount(qMin(days + 1, 8));
    axisX->setRange(QDateTime(QDate::currentDate().addDays(-(days - 1)), QTime(0, 0)),
                    QDateTime(QDate::currentDate(), QTime(23, 59, 59)));
    chart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis *axisY = new QValueAxis(chart);
    axisY->setLabelFormat("%.0f");
    axisY->setRange(0, maxY * 1.2);
    chart->addAxis(axisY, Qt::AlignLeft);

    series->attachAxis(axisX);
    series->attachAxis(axisY);

    QChartView *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    m_chartLayout->addWidget(view, 1);

    // ---- 右: 柱状图 ----
    QBarSet *barSet = new QBarSet("营收");
    QStringList cats;
    for (const auto &entry : daily) {
        barSet->append(entry.second);
        cats << entry.first.mid(5);      // MM-dd
    }

    QBarSeries *barSeries = new QBarSeries();
    barSeries->append(barSet);

    QChart *barChart = new QChart();
    barChart->setTitle(QString("每日营收对比 (近%1日)").arg(days));
    barChart->legend()->hide();
    barChart->addSeries(barSeries);

    QBarCategoryAxis *axisXc = new QBarCategoryAxis();
    axisXc->append(cats);
    barChart->addAxis(axisXc, Qt::AlignBottom);

    QValueAxis *axisYc = new QValueAxis();
    axisYc->setLabelFormat("%.0f");
    axisYc->setRange(0, maxY * 1.2);
    barChart->addAxis(axisYc, Qt::AlignLeft);

    barSeries->attachAxis(axisXc);
    barSeries->attachAxis(axisYc);

    QChartView *barView = new QChartView(barChart, this);
    barView->setRenderHint(QPainter::Antialiasing);
    m_chartLayout->addWidget(barView, 1);
#else
    // 未安装 Qt Charts: 自绘折线图 + 柱状图(降级)
    auto *plain = new PlainLineChart(this);
    plain->setData(daily);
    m_chartLayout->addWidget(plain, 1);

    auto *plainBar = new PlainBarChart(this);
    plainBar->setData(daily);
    m_chartLayout->addWidget(plainBar, 1);
#endif
}
