#include "MessagePage.h"
#include "../MessageCenter.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

namespace {
enum { TitleRole = Qt::UserRole + 1, BodyRole, TimeRole, TypeRole, ReadRole };
QString typeLabel(int type) {
    return QStringList{"通知", "系统", "订单", "退款", "预约"}.value(type,"通知");
}
// 原生绘制卡片，按当前宽度计算正文高度，长消息完整换行。
class MessageDelegate : public QStyledItemDelegate {
public:
    explicit MessageDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto *list = qobject_cast<QListWidget*>(parent());
        const int width = list ? list->viewport()->width() : option.rect.width();
        QFont font = option.font; font.setPixelSize(13);
        const int height = QFontMetrics(font).boundingRect(QRect(0,0,qMax(100,width-48),10000),
            Qt::TextWordWrap,index.data(BodyRole).toString()).height();
        return QSize(width,qMax(128, height+104));
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const bool read = index.data(ReadRole).toBool();
        const bool hovered = option.state & QStyle::State_MouseOver;
        // 普通状态略微内收；悬停时扩展并增加投影，形成列表项的“放大抬升”反馈。
        const QRect card = option.rect.adjusted(hovered ? 0 : 3,
                                                hovered ? 2 : 5,
                                                hovered ? -1 : -4,
                                                hovered ? -6 : -7);
        if (hovered) {
            p->setPen(Qt::NoPen);
            for (int i = 5; i >= 1; --i) {
                QColor shadow("#173F34");
                shadow.setAlpha(4 + i * 2);
                p->setBrush(shadow);
                p->drawRoundedRect(card.translated(0, i), 13, 13);
            }
        }
        p->setBrush(read ? QColor("#FFFFFF") : QColor("#F3FAF6"));
        p->setPen(QColor(option.state & QStyle::State_Selected ? "#6AA889" :
                         hovered ? "#78A991" : "#DDE9E2"));
        p->drawRoundedRect(card,12,12);
        const int x = card.left()+20, y = card.top()+16;
        QFont font = option.font; font.setPixelSize(12); p->setFont(font);
        p->setPen(QColor("#387459"));
        p->drawText(QRect(x,y,120,20),Qt::AlignLeft|Qt::AlignVCenter,
                    typeLabel(index.data(TypeRole).toInt()) + (read ? " · 已读" : " · 未读"));
        p->setPen(QColor("#819389"));
        p->drawText(QRect(x+120,y,card.width()-160,20),Qt::AlignRight|Qt::AlignVCenter,index.data(TimeRole).toString());
        font.setPixelSize(16); font.setBold(!read); p->setFont(font); p->setPen(QColor("#203F32"));
        p->drawText(QRect(x,y+28,card.width()-40,24),Qt::AlignVCenter,
                    QFontMetrics(font).elidedText(index.data(TitleRole).toString(),Qt::ElideRight,card.width()-40));
        font.setPixelSize(13); font.setBold(false); p->setFont(font); p->setPen(QColor("#667D70"));
        p->drawText(QRect(x,y+60,card.width()-40,card.height()-92),Qt::TextWordWrap,index.data(BodyRole).toString());
        p->restore();
    }
};
}

MessagePage::MessagePage(QWidget *parent) : QWidget(parent) {
    setObjectName("messagePage");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(28,24,28,28); layout->setSpacing(16);
    auto *title = new QLabel("消息中心",this); title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto *hint = new QLabel("充电动态与服务通知，都在这里",this); hint->setObjectName("pageHint");
    layout->addWidget(hint);
    m_summary = new QLabel(this); m_summary->setObjectName("messageSummary");
    m_summary->setWordWrap(true); layout->addWidget(m_summary);
    auto *toolbar = new QHBoxLayout;
    m_filter = new QComboBox(this); m_filter->setObjectName("messageFilter");
    m_filter->setAccessibleName("筛选消息");
    m_filter->addItem("全部消息",-1); m_filter->addItem("只看未读",0);
    for(int type=1;type<=4;++type) m_filter->addItem(typeLabel(type)+"通知",type);
    toolbar->addWidget(m_filter); toolbar->addStretch();
    m_clearBtn = new QPushButton("清空已读",this); m_clearBtn->setObjectName("secondaryBtn");
    toolbar->addWidget(m_clearBtn); layout->addLayout(toolbar);
    m_list = new QListWidget(this); m_list->setObjectName("messageList");
    m_list->setMinimumHeight(320); m_list->setResizeMode(QListView::Adjust);
    m_list->setMouseTracking(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setItemDelegate(new MessageDelegate(m_list));
    layout->addWidget(m_list,1);
    m_emptyLabel = new QLabel(this); m_emptyLabel->setObjectName("messageEmpty");
    m_emptyLabel->setAlignment(Qt::AlignCenter); m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setMinimumHeight(320); layout->addWidget(m_emptyLabel,1);
    auto *note = new QLabel("点击消息标记已读 · 清空已读不会删除未读通知",this);
    note->setObjectName("pageHint"); note->setWordWrap(true); layout->addWidget(note);
    auto read = [this](QListWidgetItem *item) { onItemClicked(m_list->row(item)); };
    connect(m_list,&QListWidget::itemClicked,this,read);
    connect(m_list,&QListWidget::itemActivated,this,read);
    connect(m_filter,qOverload<int>(&QComboBox::currentIndexChanged),this,&MessagePage::refresh);
    connect(m_clearBtn,&QPushButton::clicked,this,&MessagePage::onClearRead);
    // 合并推送和未读计数通知，避免在点击事件中销毁当前列表项。
    m_refreshTimer = new QTimer(this); m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer,&QTimer::timeout,this,&MessagePage::refresh);
    connect(&MessageCenter::instance(),&MessageCenter::messageReceived,this,[this]{m_refreshTimer->start(0);});
    connect(&MessageCenter::instance(),&MessageCenter::unreadCountChanged,this,[this]{m_refreshTimer->start(0);});
    refresh();
}
void MessagePage::refresh() {
    const auto msgs = MessageCenter::instance().messages();
    const int unread = MessageCenter::instance().unreadCount();
    m_summary->setText(unread ? QString("%1 条未读消息    /    共 %2 条通知").arg(unread).arg(msgs.size())
                             : QString("消息已全部读完    /    共 %1 条通知").arg(msgs.size()));
    m_clearBtn->setEnabled(msgs.size()>unread);
    const int scroll = m_list->verticalScrollBar()->value();
    const int selected = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toInt() : -1;
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const int filter = m_filter->currentData().toInt();
    for (const auto &m : msgs) {
        if ((filter==0 && m.read) || (filter>0 && m.type!=filter)) continue;
        auto *item = new QListWidgetItem(m.title+"\n"+m.content,m_list);
        item->setData(Qt::UserRole,m.id); item->setData(TitleRole,m.title);
        item->setData(BodyRole,m.content); item->setData(TimeRole,m.time.toString("MM-dd HH:mm"));
        item->setData(TypeRole,m.type); item->setData(ReadRole,m.read);
        if(m.id==selected) m_list->setCurrentItem(item);
    }
    m_list->verticalScrollBar()->setValue(scroll);
    const bool empty = m_list->count()==0;
    m_list->setVisible(!empty); m_emptyLabel->setVisible(empty);
    m_emptyLabel->setText(msgs.isEmpty() ? "暂时没有新消息\n\n充电进度、预约提醒和退款通知将在这里展示"
                                      : "当前分类暂无消息\n\n切换到全部消息，查看其他通知");
}
void MessagePage::refreshPage() { refresh(); }
void MessagePage::onItemClicked(int row) {
    auto *item = m_list->item(row);
    if(item && !item->data(ReadRole).toBool()) MessageCenter::instance().markRead(item->data(Qt::UserRole).toInt());
}
void MessagePage::onClearRead() { MessageCenter::instance().clearRead(); }
