#ifndef ADMINTABLECARD_H
#define ADMINTABLECARD_H
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QSplitter>

namespace AdminTableCard {
class StatusDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const QString text = index.data().toString();
        QStyleOptionViewItem base(option); initStyleOption(&base, index); base.text.clear();
        const auto *widget = option.widget;
        (widget ? widget->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &base, p, widget);
        QColor fg("#24724E"), bg("#E5F4EA");
        if (text.contains("故障") || text.contains("冻结") || text.contains("异常")) { fg=QColor("#AE454F"); bg=QColor("#FCEBED"); }
        else if (text.contains("充电") || text.contains("在用") || text.contains("排队") || text.contains("待")) { fg=QColor("#94631E"); bg=QColor("#FFF2D9"); }
        else if (text.contains("取消") || text.contains("结束")) { fg=QColor("#677B70"); bg=QColor("#EDF2EF"); }
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const int width = qMin(option.rect.width()-12, option.fontMetrics.horizontalAdvance(text)+24);
        QRect badge(option.rect.x()+8, option.rect.center().y()-12, qMax(0,width), 24);
        p->setPen(Qt::NoPen); p->setBrush(bg); p->drawRoundedRect(badge,12,12);
        p->setPen(fg); p->drawText(badge,Qt::AlignCenter,option.fontMetrics.elidedText(text,Qt::ElideRight,qMax(0,width-12)));
        p->restore();
    }
};
inline void decorate(QWidget *root) {
    for (auto *table : root->findChildren<QTableWidget*>()) {
        if (table->property("cardDecorated").toBool()) continue;
        auto *layout = table->parentWidget()->layout();
        auto *splitter = qobject_cast<QSplitter*>(table->parentWidget());
        if (!layout && !splitter) continue;
        auto *card = new QFrame;
        card->setObjectName("adminDataCard");
        card->setSizePolicy(table->sizePolicy());
        if (splitter) {
            splitter->replaceWidget(splitter->indexOf(table),card);
        } else {
            auto *old = layout->replaceWidget(table, card);
            if (!old) { delete card; continue; }
            delete old;
        }
        table->setProperty("cardDecorated",true);
        auto *body = new QVBoxLayout(card); body->setContentsMargins(16,14,16,12); body->setSpacing(12);
        auto *heading = new QHBoxLayout;
        QString title = "数据列表";
        const QString name = table->objectName();
        if (name.contains("pile",Qt::CaseInsensitive)) title="充电设备";
        else if (name.contains("station",Qt::CaseInsensitive)) title="站点档案";
        else if (name.contains("status",Qt::CaseInsensitive)) title="运行状态明细";
        else if (name.contains("order",Qt::CaseInsensitive)) title="充电订单";
        else if (table->columnCount()==9) title="排队与预约";
        else if (name.contains("user",Qt::CaseInsensitive)) title="用户档案";
        auto *label = new QLabel(title,card); label->setObjectName("dataCardTitle");
        auto *count = new QLabel(card); count->setObjectName("dataCountBadge");
        heading->addWidget(label); heading->addStretch(); heading->addWidget(count);
        body->addLayout(heading); body->addWidget(table,1);
        auto *hint = new QLabel("选择记录后可使用上方操作",card); hint->setObjectName("dataCardHint");
        body->addWidget(hint);
        const auto update = [table,count,hint] {
            count->setText(QString("%1 条记录").arg(table->rowCount()));
            hint->setText(table->objectName()=="statusTable" ? "按运行状态汇总 · 点击刷新获取最新数据"
                : table->rowCount() ? "选择记录后可使用上方操作" : "暂无记录，可调整筛选条件或刷新列表");
        };
        QObject::connect(table->model(), &QAbstractItemModel::rowsInserted, count, update);
        QObject::connect(table->model(), &QAbstractItemModel::rowsRemoved, count, update);
        QObject::connect(table->model(), &QAbstractItemModel::modelReset, count, update); update();
        table->setShowGrid(false); table->setFrameShape(QFrame::NoFrame);
        table->setAlternatingRowColors(true); table->setMouseTracking(true);
        table->verticalHeader()->setDefaultSectionSize(44);
        table->horizontalHeader()->setMinimumHeight(40);
        if (table->columnCount() > 0)
            table->horizontalHeader()->setSectionResizeMode(table->columnCount() - 1,
                                                             QHeaderView::Stretch);
        for (int c=0;c<table->columnCount();++c) {
            if (table->horizontalHeaderItem(c) && table->horizontalHeaderItem(c)->text().contains("状态"))
                table->setItemDelegateForColumn(c,new StatusDelegate(table));
        }
    }
}
}
#endif
