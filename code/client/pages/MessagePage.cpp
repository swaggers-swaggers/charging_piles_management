#include "MessagePage.h"
#include "../MessageCenter.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

static QString typeColor(int type)
{
    switch (type) {
    case 3: return QStringLiteral("#D4380D");   // 退款-红
    case 2: return QStringLiteral("#1677FF");   // 订单-蓝
    case 4: return QStringLiteral("#722ED1");   // 预约-紫
    case 5: return QStringLiteral("#FA8C16");   // 排队-橙
    default: return QStringLiteral("#595959");
    }
}

static QString typeLabel(int type)
{
    switch (type) {
    case 1: return QStringLiteral("系统");
    case 2: return QStringLiteral("订单");
    case 3: return QStringLiteral("退款");
    case 4: return QStringLiteral("预约");
    case 5: return QStringLiteral("排队");
    default: return QStringLiteral("通知");
    }
}

MessagePage::MessagePage(QWidget *parent)
    : QWidget(parent)
{
    auto *top = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("消息中心"), this);
    title->setStyleSheet("font-size:18px;font-weight:bold;color:#1A1B1C;");
    m_clearBtn = new QPushButton(QStringLiteral("清空已读"), this);
    m_clearBtn->setStyleSheet("QPushButton{background:#F5F5F5;border:1px solid #E4E3DD;"
                               "border-radius:6px;padding:6px 14px;color:#595959;}"
                               "QPushButton:hover{background:#EAEAEA;}");
    top->addWidget(title);
    top->addStretch();
    top->addWidget(m_clearBtn);

    m_list = new QListWidget(this);
    m_list->setStyleSheet("QListWidget{border:1px solid #E4E3DD;border-radius:8px;"
                           "background:#FFFFFF;outline:none;}"
                           "QListWidget::item{padding:12px 14px;border-bottom:1px solid #F0F0F0;}"
                           "QListWidget::item:selected{background:#F0F7FF;}");

    m_emptyLabel = new QLabel(QStringLiteral("暂无消息"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color:#BFBFBF;font-size:14px;padding:40px;");
    m_emptyLabel->hide();

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(12);
    lay->addLayout(top);
    lay->addWidget(m_list, 1);
    lay->addWidget(m_emptyLabel);

    connect(m_list, &QListWidget::currentRowChanged, this, &MessagePage::onItemClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &MessagePage::onClearRead);
    connect(&MessageCenter::instance(), &MessageCenter::messageReceived,
            this, &MessagePage::refresh);
    connect(&MessageCenter::instance(), &MessageCenter::unreadCountChanged,
            this, &MessagePage::refresh);

    refresh();
}

void MessagePage::refresh()
{
    const auto msgs = MessageCenter::instance().messages();
    m_list->clear();
    if (msgs.isEmpty()) {
        m_emptyLabel->show();
        m_list->hide();
        return;
    }
    m_emptyLabel->hide();
    m_list->show();
    for (const AppMessage &m : msgs) {
        const QString color = typeColor(m.type);
        const QString dot = m.read
            ? QString()
            : QStringLiteral("<span style='display:inline-block;width:8px;height:8px;"
                             "border-radius:4px;background:%1;margin-right:6px;'></span>").arg(color);
        const QString html =
            QStringLiteral(
                "<div style='%1'>"
                "<div style='display:flex;align-items:center;gap:6px;'>"
                "%2"
                "<span style='font-weight:bold;color:%3;'>[%4]</span>"
                "<span style='font-weight:bold;color:#1A1B1C;'>%5</span>"
                "<span style='flex:1;'></span>"
                "<span style='color:#BFBFBF;font-size:11px;'>%6</span>"
                "</div>"
                "<div style='color:%7;margin-top:4px;font-size:13px;'>%8</div>"
                "</div>")
                .arg(m.read ? QString() : QStringLiteral("background:#FAFAFA;"),
                     dot, color, typeLabel(m.type), m.title.toHtmlEscaped(),
                     m.time.toString(QStringLiteral("MM-dd HH:mm")),
                     m.read ? QStringLiteral("#8C8C8C") : QStringLiteral("#434343"),
                     m.content.toHtmlEscaped());
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, m.id);
        m_list->addItem(item);
        auto *lbl = new QLabel(html, m_list);
        lbl->setWordWrap(true);
        lbl->setTextFormat(Qt::RichText);
        m_list->setItemWidget(item, lbl);
    }
}

void MessagePage::onItemClicked(int row)
{
    if (row < 0) return;
    auto *item = m_list->item(row);
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    MessageCenter::instance().markRead(id);
}

void MessagePage::onClearRead()
{
    MessageCenter::instance().clearRead();
    refresh();
}
