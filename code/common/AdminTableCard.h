#ifndef ADMINTABLECARD_H
#define ADMINTABLECARD_H

#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

// 双端共用的数据表视觉：外层数据卡片、整行卡片、内嵌指标条、三态徽章，
// 以及数据刷新/新记录的短动画。业务层仍可直接读取 item()->text()。
namespace AdminTableCard {

enum DataRole {
    FlashUntilRole = Qt::UserRole + 80,
    InsertedAtRole,
};

inline qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

inline void markUpdated(QTableWidgetItem *item, int durationMs = 720)
{
    if (item)
        item->setData(FlashUntilRole, nowMs() + durationMs);
}

inline void markInserted(QTableWidgetItem *item, qint64 startedAt = 0)
{
    if (item)
        item->setData(InsertedAtRole, startedAt > 0 ? startedAt : nowMs());
}

class CellDelegate : public QStyledItemDelegate
{
public:
    enum Kind { Plain, Status, Percent, Energy, Power, Amount };

    explicit CellDelegate(Kind kind, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_kind(kind)
    {
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const qint64 current = nowMs();
        const qint64 insertedAt = index.data(InsertedAtRole).toLongLong();
        const qreal entry = insertedAt > 0
            ? qBound<qreal>(0.0, (current - insertedAt) / 360.0, 1.0) : 1.0;

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setClipRect(option.rect);
        p->setOpacity(entry);
        p->translate(0, -10.0 * (1.0 - entry));

        const bool firstColumn = index.column() == 0;
        const bool lastColumn = index.column() == index.model()->columnCount(index.parent()) - 1;
        QRectF tile = QRectF(option.rect).adjusted(firstColumn ? 5 : 0, 4,
                                                   lastColumn ? -5 : 0, -4);
        QColor tileColor = index.row() % 2 ? QColor("#F7FAF8") : QColor("#FFFFFF");
        if (option.state & QStyle::State_Selected)
            tileColor = QColor("#E3F2EA");
        else if (option.state & QStyle::State_MouseOver)
            tileColor = QColor("#EEF7F2");

        const qint64 flashUntil = index.data(FlashUntilRole).toLongLong();
        if (flashUntil > current) {
            const qreal remaining = qBound<qreal>(0.0, (flashUntil - current) / 720.0, 1.0);
            tileColor = blend(tileColor, QColor("#D8ECFF"), 0.38 + remaining * 0.46);
        }

        paintRowCardSegment(p, tile, tileColor, firstColumn, lastColumn);

        if (m_kind == Status)
            paintStatus(p, option, index, tile, current);
        else if (m_kind == Percent || m_kind == Energy || m_kind == Power || m_kind == Amount)
            paintMetric(p, option, index, tile);
        else
            paintText(p, option, index, tile);
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize result = QStyledItemDelegate::sizeHint(option, index);
        result.setHeight(qMax(result.height(), 42));
        if (m_kind == Status)
            result.setWidth(result.width() + 36);
        else if (m_kind == Energy || m_kind == Power || m_kind == Amount)
            result.setWidth(result.width() + 34);
        else if (m_kind == Percent)
            result.setWidth(result.width() + 22);
        return result;
    }

private:
    static void paintRowCardSegment(QPainter *p, const QRectF &rect, const QColor &color,
                                    bool first, bool last)
    {
        constexpr qreal radius = 10.0;
        QPainterPath fill;
        if (first && last) {
            fill.addRoundedRect(rect, radius, radius);
        } else if (first) {
            fill.moveTo(rect.right(), rect.top());
            fill.lineTo(rect.left() + radius, rect.top());
            fill.quadTo(rect.left(), rect.top(), rect.left(), rect.top() + radius);
            fill.lineTo(rect.left(), rect.bottom() - radius);
            fill.quadTo(rect.left(), rect.bottom(), rect.left() + radius, rect.bottom());
            fill.lineTo(rect.right(), rect.bottom());
            fill.closeSubpath();
        } else if (last) {
            fill.moveTo(rect.left(), rect.top());
            fill.lineTo(rect.right() - radius, rect.top());
            fill.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radius);
            fill.lineTo(rect.right(), rect.bottom() - radius);
            fill.quadTo(rect.right(), rect.bottom(), rect.right() - radius, rect.bottom());
            fill.lineTo(rect.left(), rect.bottom());
            fill.closeSubpath();
        } else {
            fill.addRect(rect);
        }
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawPath(fill);

        QPainterPath outline;
        if (first) {
            outline.moveTo(rect.right(), rect.top());
            outline.lineTo(rect.left() + radius, rect.top());
            outline.quadTo(rect.left(), rect.top(), rect.left(), rect.top() + radius);
            outline.lineTo(rect.left(), rect.bottom() - radius);
            outline.quadTo(rect.left(), rect.bottom(), rect.left() + radius, rect.bottom());
            outline.lineTo(rect.right(), rect.bottom());
        } else if (last) {
            outline.moveTo(rect.left(), rect.top());
            outline.lineTo(rect.right() - radius, rect.top());
            outline.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radius);
            outline.lineTo(rect.right(), rect.bottom() - radius);
            outline.quadTo(rect.right(), rect.bottom(), rect.right() - radius, rect.bottom());
            outline.lineTo(rect.left(), rect.bottom());
        } else {
            outline.moveTo(rect.left(), rect.top());
            outline.lineTo(rect.right(), rect.top());
            outline.moveTo(rect.left(), rect.bottom());
            outline.lineTo(rect.right(), rect.bottom());
        }
        p->setPen(QPen(QColor("#DFE9E3"), 1));
        p->setBrush(Qt::NoBrush);
        p->drawPath(outline);
    }

    static QColor blend(const QColor &from, const QColor &to, qreal amount)
    {
        amount = qBound<qreal>(0.0, amount, 1.0);
        return QColor(qRound(from.red() + (to.red() - from.red()) * amount),
                      qRound(from.green() + (to.green() - from.green()) * amount),
                      qRound(from.blue() + (to.blue() - from.blue()) * amount));
    }

    static double numberFrom(const QString &text)
    {
        QString normalized = text;
        normalized.remove('%').remove(',').remove(QStringLiteral("¥"));
        normalized.remove(QStringLiteral("度")).remove(QStringLiteral("元"));
        normalized.remove(QStringLiteral("kW"), Qt::CaseInsensitive);
        bool ok = false;
        const double value = normalized.trimmed().toDouble(&ok);
        return ok ? value : 0.0;
    }

    void paintText(QPainter *p, const QStyleOptionViewItem &option,
                   const QModelIndex &index, const QRectF &tile) const
    {
        const QString text = index.data(Qt::DisplayRole).toString();
        const QRect textRect = tile.adjusted(10, 0, -10, 0).toRect();
        p->setPen((option.state & QStyle::State_Selected) ? QColor("#174D36")
                                                          : QColor("#344C40"));
        p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                    option.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width()));
    }

    void paintStatus(QPainter *p, const QStyleOptionViewItem &option,
                     const QModelIndex &index, const QRectF &tile, qint64 current) const
    {
        const QString text = index.data(Qt::DisplayRole).toString();
        QColor fg("#24724E"), bg("#E5F4EA"), border("#CBE7D5");
        const bool fault = text.contains(QStringLiteral("故障"))
                           || text.contains(QStringLiteral("冻结"))
                           || text.contains(QStringLiteral("异常"));
        const bool charging = text.contains(QStringLiteral("充电中"))
                              || text.contains(QStringLiteral("使用中"))
                              || text.contains(QStringLiteral("在用"));
        if (fault) {
            fg = QColor("#B23D4A"); bg = QColor("#FCE8EB"); border = QColor("#F1C6CC");
        } else if (charging) {
            const qreal pulse = (qSin(current / 260.0) + 1.0) / 2.0;
            fg = QColor("#1761C2");
            bg = blend(QColor("#E7F1FF"), QColor("#BFD9FF"), pulse * 0.55);
            border = blend(QColor("#C5DAF7"), QColor("#76A9F4"), pulse * 0.65);
        } else if (text.contains(QStringLiteral("取消"))
                   || text.contains(QStringLiteral("结束"))
                   || text.contains(QStringLiteral("过期"))) {
            fg = QColor("#677B70"); bg = QColor("#EDF2EF"); border = QColor("#DDE6E1");
        } else if (text.contains(QStringLiteral("待")) || text.contains(QStringLiteral("预约"))) {
            fg = QColor("#1761C2"); bg = QColor("#E7F1FF"); border = QColor("#C5DAF7");
        }

        const int textWidth = option.fontMetrics.horizontalAdvance(text);
        const qreal badgeWidth = qMin(tile.width() - 12.0, qreal(textWidth + 32));
        QRectF badge(tile.left() + 7, tile.center().y() - 12, qMax<qreal>(0, badgeWidth), 24);
        p->setPen(QPen(border, 1));
        p->setBrush(bg);
        p->drawRoundedRect(badge, 12, 12);
        p->setPen(fg);
        p->drawEllipse(QRectF(badge.left() + 9, badge.center().y() - 3, 6, 6));
        QRect textRect = badge.adjusted(19, 0, -7, 0).toRect();
        p->drawText(textRect, Qt::AlignCenter,
                    option.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width()));
    }

    void paintMetric(QPainter *p, const QStyleOptionViewItem &option,
                     const QModelIndex &index, const QRectF &tile) const
    {
        const QString raw = index.data(Qt::DisplayRole).toString();
        const double value = numberFrom(raw);
        double maximum = 100.0;
        if (m_kind != Percent) {
            maximum = 0.0;
            for (int row = 0; row < index.model()->rowCount(index.parent()); ++row)
                maximum = qMax(maximum, numberFrom(index.sibling(row, index.column())
                                                        .data(Qt::DisplayRole).toString()));
            if (maximum <= 0.0)
                maximum = 1.0;
        }
        const qreal ratio = qBound<qreal>(0.0, value / maximum, 1.0);
        QRectF track = tile.adjusted(8, 8, -8, -8);
        QColor fill("#CFE7FF"), textColor("#285C88");
        if (m_kind == Amount) {
            fill = QColor("#DCEFE4"); textColor = QColor("#246247");
        }
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#EDF3F7"));
        p->drawRoundedRect(track, 7, 7);
        if (ratio > 0.001) {
            QRectF progress = track;
            progress.setWidth(qMax<qreal>(8.0, track.width() * ratio));
            p->setBrush(fill);
            p->drawRoundedRect(progress, 7, 7);
        }

        QString display = raw;
        if (m_kind == Energy && !raw.contains(QStringLiteral("度")))
            display += QStringLiteral(" 度");
        else if (m_kind == Power && !raw.contains(QStringLiteral("kW"), Qt::CaseInsensitive))
            display += QStringLiteral(" kW");
        else if (m_kind == Amount && !raw.trimmed().isEmpty()
                 && !raw.contains(QStringLiteral("¥")))
            display = QStringLiteral("¥ ") + raw;
        p->setPen(textColor);
        QFont font = option.font;
        font.setBold(true);
        p->setFont(font);
        p->drawText(track.adjusted(9, 0, -8, 0).toRect(), Qt::AlignVCenter | Qt::AlignLeft,
                    option.fontMetrics.elidedText(display, Qt::ElideRight,
                                                  qMax(0, int(track.width()) - 17)));
    }

    Kind m_kind = Plain;
};

inline CellDelegate::Kind kindForHeader(const QString &header)
{
    if (header.contains(QStringLiteral("状态")))
        return CellDelegate::Status;
    if (header.contains(QStringLiteral("占比")) || header.contains(QStringLiteral("占用率"))
        || header.contains(QStringLiteral("在线率")) || header.contains(QStringLiteral("百分比")))
        return CellDelegate::Percent;
    if (header.contains(QStringLiteral("电量")))
        return CellDelegate::Energy;
    if (header.contains(QStringLiteral("功率")))
        return CellDelegate::Power;
    if (header.contains(QStringLiteral("金额")) || header.contains(QStringLiteral("消费"))
        || header.contains(QStringLiteral("已退")))
        return CellDelegate::Amount;
    return CellDelegate::Plain;
}

inline void configureTable(QTableWidget *table)
{
    if (!table || table->property("cellCardConfigured").toBool())
        return;
    table->setProperty("cellCardConfigured", true);
    table->setProperty("rowCardConfigured", true);
    table->setShowGrid(false);
    table->setFrameShape(QFrame::NoFrame);
    table->setAlternatingRowColors(false);
    table->setMouseTracking(true);
    table->setWordWrap(false);
    table->verticalHeader()->setDefaultSectionSize(52);
    table->horizontalHeader()->setMinimumHeight(42);
    if (table->columnCount() > 0)
        table->horizontalHeader()->setSectionResizeMode(table->columnCount() - 1,
                                                         QHeaderView::Stretch);
    for (int column = 0; column < table->columnCount(); ++column) {
        const QString header = table->horizontalHeaderItem(column)
            ? table->horizontalHeaderItem(column)->text() : QString();
        table->setItemDelegateForColumn(column,
                                        new CellDelegate(kindForHeader(header), table));
    }
    auto *timer = new QTimer(table);
    timer->setObjectName(QStringLiteral("dataCellAnimationTimer"));
    timer->setInterval(80);
    QObject::connect(timer, &QTimer::timeout, table, [table] {
        if (table->isVisible() && table->viewport())
            table->viewport()->update();
    });
    timer->start();
}

inline void decorate(QWidget *root)
{
    for (auto *table : root->findChildren<QTableWidget *>()) {
        if (table->property("cardDecorated").toBool())
            continue;
        auto *layout = table->parentWidget()->layout();
        auto *splitter = qobject_cast<QSplitter *>(table->parentWidget());
        if (!layout && !splitter)
            continue;
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("adminDataCard"));
        card->setSizePolicy(table->sizePolicy());
        if (splitter) {
            splitter->replaceWidget(splitter->indexOf(table), card);
        } else {
            auto *old = layout->replaceWidget(table, card);
            if (!old) {
                delete card;
                continue;
            }
            delete old;
        }
        table->setProperty("cardDecorated", true);
        auto *body = new QVBoxLayout(card);
        body->setContentsMargins(16, 14, 16, 12);
        body->setSpacing(12);
        auto *heading = new QHBoxLayout;
        QString title = table->property("cardTitle").toString();
        if (title.isEmpty()) {
            title = QStringLiteral("数据列表");
            const QString name = table->objectName();
            if (name.contains(QStringLiteral("pile"), Qt::CaseInsensitive))
                title = QStringLiteral("充电设备");
            else if (name.contains(QStringLiteral("station"), Qt::CaseInsensitive))
                title = QStringLiteral("站点档案");
            else if (name.contains(QStringLiteral("status"), Qt::CaseInsensitive))
                title = QStringLiteral("运行状态明细");
            else if (name.contains(QStringLiteral("order"), Qt::CaseInsensitive))
                title = QStringLiteral("充电订单");
            else if (table->columnCount() == 9)
                title = QStringLiteral("时段预约");
            else if (name.contains(QStringLiteral("user"), Qt::CaseInsensitive))
                title = QStringLiteral("用户档案");
        }
        auto *label = new QLabel(title, card);
        label->setObjectName(QStringLiteral("dataCardTitle"));
        auto *count = new QLabel(card);
        count->setObjectName(QStringLiteral("dataCountBadge"));
        heading->addWidget(label);
        heading->addStretch();
        heading->addWidget(count);
        body->addLayout(heading);
        body->addWidget(table, 1);
        auto *hint = new QLabel(table->property("cardHint").toString(), card);
        hint->setObjectName(QStringLiteral("dataCardHint"));
        body->addWidget(hint);
        const auto update = [table, count, hint] {
            count->setText(QStringLiteral("%1 条记录").arg(table->rowCount()));
            const QString customHint = table->property("cardHint").toString();
            hint->setText(!customHint.isEmpty() ? customHint
                : table->objectName() == QStringLiteral("statusTable")
                    ? QStringLiteral("比例以单元格内进度条呈现 · 点击刷新获取最新数据")
                    : table->rowCount()
                        ? QStringLiteral("选择记录后可使用上方操作")
                        : QStringLiteral("暂无记录，可调整筛选条件或刷新列表"));
        };
        QObject::connect(table->model(), &QAbstractItemModel::rowsInserted, count, update);
        QObject::connect(table->model(), &QAbstractItemModel::rowsRemoved, count, update);
        QObject::connect(table->model(), &QAbstractItemModel::modelReset, count, update);
        configureTable(table);
        update();
    }
}

} // namespace AdminTableCard

#endif // ADMINTABLECARD_H
