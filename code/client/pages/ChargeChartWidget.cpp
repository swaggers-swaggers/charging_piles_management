#include "ChargeChartWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

ChargeChartWidget::ChargeChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background:#FFFFFF;border:1px solid #E4E3DD;border-radius:10px;");
    setMinimumHeight(140);
}

void ChargeChartWidget::addPoint(int minutes, double energy, double amount)
{
    // 同一分钟只保留最新值(避免重复点)
    if (!m_data.isEmpty() && m_data.last().minutes == minutes) {
        m_data.last().energy = energy;
        m_data.last().amount = amount;
    } else {
        m_data.append({minutes, energy, amount});
    }
    update();
}

void ChargeChartWidget::clearData()
{
    m_data.clear();
    update();
}

void ChargeChartWidget::setMode(int mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        update();
    }
}

void ChargeChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const int padL = 44, padR = 14, padT = 22, padB = 26;
    const int cw = w - padL - padR;
    const int ch = h - padT - padB;

    // 标题
    p.setPen(QColor("#595959"));
    p.setFont(QFont(QString(), 10, QFont::Bold));
    p.drawText(padL, 4, cw, 16, Qt::AlignLeft | Qt::AlignVCenter,
               m_mode == 0 ? QStringLiteral("充电曲线 — 电量(度)")
                           : QStringLiteral("充电曲线 — 金额(元)"));

    if (m_data.size() < 2) {
        p.setPen(QColor("#BFBFBF"));
        p.setFont(QFont(QString(), 11));
        p.drawText(padL, padT, cw, ch, Qt::AlignCenter,
                   QStringLiteral("充电中, 数据采集中..."));
        return;
    }

    // 计算范围
    int maxMin = m_data.last().minutes;
    if (maxMin < 10) maxMin = 10;
    double maxVal = 0;
    for (const auto &pt : m_data)
        maxVal = qMax(maxVal, m_mode == 0 ? pt.energy : pt.amount);
    if (maxVal < 1.0) maxVal = 1.0;
    // Y轴向上取整到合适刻度
    double yStep = 1.0;
    if (maxVal > 50) yStep = 10.0;
    else if (maxVal > 20) yStep = 5.0;
    else if (maxVal > 10) yStep = 2.0;
    double yMax = ceil(maxVal / yStep) * yStep;

    auto xOf = [&](int min) { return padL + (double)min / maxMin * cw; };
    auto yOf = [&](double val) { return padT + ch - (val / yMax) * ch; };

    // 网格线 + Y轴标签
    p.setFont(QFont(QString(), 9));
    p.setPen(QPen(QColor("#F0F0F0"), 1));
    for (double v = 0; v <= yMax + 0.001; v += yStep) {
        const int y = yOf(v);
        p.drawLine(padL, y, padL + cw, y);
        p.setPen(QColor("#8C8C8C"));
        p.drawText(0, y - 8, padL - 6, 16, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', v < 10 ? 1 : 0));
        p.setPen(QPen(QColor("#F0F0F0"), 1));
    }

    // X轴标签(最多5个)
    p.setPen(QColor("#8C8C8C"));
    const int xTicks = qMin(5, maxMin);
    for (int i = 0; i <= xTicks; ++i) {
        const int min = qRound((double)i / xTicks * maxMin);
        const int x = xOf(min);
        p.drawText(x - 20, padT + ch + 4, 40, 16, Qt::AlignHCenter | Qt::AlignTop,
                   QString("%1分").arg(min));
    }

    // 渐变填充区域
    const QColor lineColor = m_mode == 0 ? QColor("#1677FF") : QColor("#FA8C16");
    QLinearGradient grad(0, padT, 0, padT + ch);
    grad.setColorAt(0, QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 40));
    grad.setColorAt(1, QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 4));

    QPainterPath path;
    path.moveTo(xOf(m_data.first().minutes), yOf(m_mode == 0 ? m_data.first().energy : m_data.first().amount));
    for (int i = 1; i < m_data.size(); ++i) {
        const auto &pt = m_data[i];
        path.lineTo(xOf(pt.minutes), yOf(m_mode == 0 ? pt.energy : pt.amount));
    }
    // 闭合填充
    QPainterPath fillPath = path;
    fillPath.lineTo(xOf(m_data.last().minutes), padT + ch);
    fillPath.lineTo(xOf(m_data.first().minutes), padT + ch);
    fillPath.closeSubpath();
    p.fillPath(fillPath, grad);

    // 折线
    p.setPen(QPen(lineColor, 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // 数据点
    p.setBrush(lineColor);
    p.setPen(Qt::NoPen);
    for (const auto &pt : m_data) {
        p.drawEllipse(QPointF(xOf(pt.minutes),
                               yOf(m_mode == 0 ? pt.energy : pt.amount)), 3, 3);
    }

    // 最新值标签
    const auto &last = m_data.last();
    const double lastVal = m_mode == 0 ? last.energy : last.amount;
    const QString tag = QString("%1 %2").arg(lastVal, 0, 'f', 2)
                            .arg(m_mode == 0 ? QStringLiteral("度") : QStringLiteral("元"));
    const QFontMetrics fm(p.font());
    const int tw = fm.horizontalAdvance(tag) + 12;
    const int lx = qMin(padL + cw - tw, (int)(xOf(last.minutes) - tw / 2.0));
    const int ly = yOf(lastVal) - 24;
    p.setBrush(QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 220));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(lx, ly, tw, 20, 4, 4);
    p.setPen(Qt::white);
    p.setFont(QFont(QString(), 9, QFont::Bold));
    p.drawText(lx, ly, tw, 20, Qt::AlignCenter, tag);
}
