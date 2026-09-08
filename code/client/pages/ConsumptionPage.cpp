#include "ConsumptionPage.h"

#include "ClientSession.h"
#include "protocol.h"
#include "types.h"
#include "network/TcpClient.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
void clearLayout(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        else if (item->layout()) delete item->layout();
        delete item;
    }
}

QLabel *makeLabel(const QString &text, const QString &name, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(name);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    return l;
}

QFrame *makeMetric(const QString &caption, const QString &value, QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName("consumeMetric");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);
    layout->addWidget(makeLabel(caption, "consumeMetricCaption", panel));
    layout->addWidget(makeLabel(value, "consumeMetricValue", panel));
    return panel;
}

QString compactDate(const QString &value)
{
    if (value.trimmed().isEmpty()) return QStringLiteral("--");
    const QDateTime t = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return t.isValid() ? t.toString(QStringLiteral("MM.dd HH:mm")) : value;
}
} // namespace

ConsumptionPage::ConsumptionPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("consumptionPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 28);
    layout->setSpacing(16);

    layout->addWidget(makeLabel(QStringLiteral("我的历史消费"), "pageTitle", this));
    layout->addWidget(makeLabel(QStringLiteral("每一笔补能，都值得被认真记录"), "pageHint", this));

    // 累计消费 Hero
    auto *hero = new QFrame(this);
    hero->setObjectName("consumeHero");
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(26, 22, 26, 22);
    heroLayout->setSpacing(6);
    heroLayout->addWidget(makeLabel(QStringLiteral("累计充电消费 · 元"), "consumeHeroCaption", hero));
    m_heroAmount = makeLabel(QStringLiteral("¥ 0.00"), "consumeHeroAmount", hero);
    heroLayout->addWidget(m_heroAmount);
    heroLayout->addWidget(makeLabel(QStringLiteral("统计已完成充电订单"), "consumeHeroHint", hero));
    layout->addWidget(hero);

    // 指标行
    auto *metrics = new QHBoxLayout;
    metrics->setSpacing(12);
    auto *m1 = makeMetric(QStringLiteral("本月消费"), QStringLiteral("--"), this);
    auto *m2 = makeMetric(QStringLiteral("充电次数"), QStringLiteral("--"), this);
    auto *m3 = makeMetric(QStringLiteral("累计电量"), QStringLiteral("--"), this);
    auto *m4 = makeMetric(QStringLiteral("累计时长"), QStringLiteral("--"), this);
    // 保留值标签指针便于刷新
    m_metricMonth = m1->findChild<QLabel *>("consumeMetricValue");
    m_metricOrders = m2->findChild<QLabel *>("consumeMetricValue");
    m_metricEnergy = m3->findChild<QLabel *>("consumeMetricValue");
    m_metricMinutes = m4->findChild<QLabel *>("consumeMetricValue");
    metrics->addWidget(m1, 1);
    metrics->addWidget(m2, 1);
    metrics->addWidget(m3, 1);
    metrics->addWidget(m4, 1);
    layout->addLayout(metrics);

    // 月度趋势
    auto *trendSection = new QFrame(this);
    trendSection->setObjectName("consumeSection");
    auto *trendLayout = new QVBoxLayout(trendSection);
    trendLayout->setContentsMargins(22, 18, 22, 20);
    trendLayout->setSpacing(12);
    trendLayout->addWidget(makeLabel(QStringLiteral("近 6 个月消费趋势"), "consumeSectionTitle", trendSection));
    m_monthly = new QVBoxLayout;
    m_monthly->setSpacing(9);
    trendLayout->addLayout(m_monthly);
    layout->addWidget(trendSection);

    // 最近消费记录
    auto *recordSection = new QFrame(this);
    recordSection->setObjectName("consumeSection");
    auto *recordLayout = new QVBoxLayout(recordSection);
    recordLayout->setContentsMargins(22, 18, 22, 20);
    recordLayout->setSpacing(12);
    recordLayout->addWidget(makeLabel(QStringLiteral("最近消费记录"), "consumeSectionTitle", recordSection));
    m_records = new QVBoxLayout;
    m_records->setSpacing(10);
    recordLayout->addLayout(m_records);
    layout->addWidget(recordSection);

    layout->addStretch();
}

void ConsumptionPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &ConsumptionPage::refresh);
}

void ConsumptionPage::refreshPage()
{
    m_silentRefresh = true;
    refresh();
    m_silentRefresh = false;
}

void ConsumptionPage::refresh()
{
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqConsumptionSummary, QJsonObject{{"userId", ClientSession::instance().userId}});
    if (!reply.value("ok").toBool()) {
        if (!m_silentRefresh)
            QMessageBox::warning(this, QStringLiteral("加载失败"), reply.value("error").toString());
        return;
    }

    const double totalSpent = reply.value("totalSpent").toDouble();
    const double totalEnergy = reply.value("totalEnergy").toDouble();
    const int totalOrders = reply.value("totalOrders").toInt();
    const double totalMinutes = reply.value("totalMinutes").toDouble();
    const double monthSpent = reply.value("monthSpent").toDouble();

    m_heroAmount->setText(QStringLiteral("¥ %1").arg(totalSpent, 0, 'f', 2));
    m_metricMonth->setText(QStringLiteral("¥ %1").arg(monthSpent, 0, 'f', 2));
    m_metricOrders->setText(QStringLiteral("%1 次").arg(totalOrders));
    m_metricEnergy->setText(QStringLiteral("%1 度").arg(totalEnergy, 0, 'f', 1));
    m_metricMinutes->setText(QStringLiteral("%1 分钟").arg(totalMinutes, 0, 'f', 0));

    // 月度趋势: 计算最大月消费, 生成横向条形
    const QJsonArray monthly = reply.value("monthly").toArray();
    double maxSpent = 0.0;
    for (const QJsonValue &value : monthly)
        maxSpent = qMax(maxSpent, value.toObject().value("spent").toDouble());
    if (maxSpent <= 0) maxSpent = 1.0;

    clearLayout(m_monthly);
    if (monthly.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无消费记录"), this);
        empty->setObjectName("pageHint");
        m_monthly->addWidget(empty);
    } else {
        for (const QJsonValue &value : monthly) {
            const QJsonObject o = value.toObject();
            const QString month = o.value("month").toString();
            const double spent = o.value("spent").toDouble();
            auto *row = new QHBoxLayout;
            row->setSpacing(10);
            auto *monthLabel = new QLabel(month, this);
            monthLabel->setObjectName("consumeBarMonth");
            monthLabel->setFixedWidth(62);
            auto *track = new QFrame(this);
            track->setObjectName("consumeBarTrack");
            auto *trackLayout = new QHBoxLayout(track);
            trackLayout->setContentsMargins(0, 0, 0, 0);
            trackLayout->setSpacing(0);
            auto *fill = new QFrame(track);
            fill->setObjectName("consumeBarFill");
            fill->setFixedWidth(qMax(4, int(spent / maxSpent * 280)));
            trackLayout->addWidget(fill);
            trackLayout->addStretch();
            auto *amount = new QLabel(QStringLiteral("¥ %1").arg(spent, 0, 'f', 2), this);
            amount->setObjectName("consumeBarAmount");
            row->addWidget(monthLabel);
            row->addWidget(track, 1);
            row->addWidget(amount);
            m_monthly->addLayout(row);
        }
    }

    // 最近消费记录
    clearLayout(m_records);
    const QJsonArray recent = reply.value("recent").toArray();
    if (recent.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("还没有消费记录\n\n完成一次充电后，这里会出现明细"), this);
        empty->setObjectName("orderEmpty");
        empty->setAlignment(Qt::AlignCenter);
        m_records->addWidget(empty);
    } else {
        for (const QJsonValue &value : recent) {
            const OrderInfo order = OrderInfo::fromJson(value.toObject());
            auto *card = new QFrame(this);
            card->setObjectName("consumeRecordCard");
            auto *row = new QHBoxLayout(card);
            row->setContentsMargins(16, 12, 16, 12);
            row->setSpacing(14);
            auto *date = new QLabel(compactDate(order.startTime), card);
            date->setObjectName("consumeRecordDate");
            auto *station = new QLabel(order.stationName.isEmpty() ? QStringLiteral("未命名充电站")
                                                                   : order.stationName, card);
            station->setObjectName("consumeRecordStation");
            auto *energy = new QLabel(QStringLiteral("%1 度").arg(order.energy, 0, 'f', 1), card);
            energy->setObjectName("consumeRecordEnergy");
            auto *amount = new QLabel(QStringLiteral("¥ %1").arg(order.amount, 0, 'f', 2), card);
            amount->setObjectName("consumeRecordAmount");
            row->addWidget(date);
            row->addWidget(station, 1);
            row->addWidget(energy);
            row->addWidget(amount);
            m_records->addWidget(card);
        }
    }
}
