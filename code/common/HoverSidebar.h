#ifndef HOVERSIDEBAR_H
#define HOVERSIDEBAR_H

#include <QEnterEvent>
#include <QListWidget>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QWidget>

// 悬停展开的共用侧栏。收起时只保留图标，既不占用内容区，也不会通过
// 改写业务标题影响页面切换；完整标题单独保存在 FullTextRole。
class HoverSidebar : public QWidget
{
public:
    enum { FullTextRole = Qt::UserRole + 42 };

    explicit HoverSidebar(int expandedWidth, QWidget *parent = nullptr)
        : QWidget(parent), m_expandedWidth(expandedWidth)
    {
        setObjectName(QStringLiteral("sidebar"));
        setProperty("sidebarExpanded", false);
        setMouseTracking(true);
        setMinimumWidth(m_collapsedWidth);
        setMaximumWidth(m_collapsedWidth);

        m_widthAnimation = new QParallelAnimationGroup(this);
        m_minimumAnimation = new QPropertyAnimation(this, "minimumWidth", m_widthAnimation);
        m_maximumAnimation = new QPropertyAnimation(this, "maximumWidth", m_widthAnimation);
        for (auto *animation : {m_minimumAnimation, m_maximumAnimation}) {
            animation->setDuration(190);
            animation->setEasingCurve(QEasingCurve::OutCubic);
            m_widthAnimation->addAnimation(animation);
        }
    }

    void setNavigationList(QListWidget *list)
    {
        m_navigation = list;
        if (!m_navigation) return;
        m_navigation->setProperty("sidebarExpanded", false);
        for (int row = 0; row < m_navigation->count(); ++row) {
            auto *item = m_navigation->item(row);
            if (!item->data(FullTextRole).isValid())
                item->setData(FullTextRole, item->text());
            item->setToolTip(item->data(FullTextRole).toString());
            item->setText(QString());
        }
    }

    void addExpandedOnly(QWidget *widget)
    {
        if (!widget) return;
        m_expandedOnly.append(widget);
        widget->hide();
    }

    void setActionButton(QWidget *button, const QString &expandedText)
    {
        m_actionButton = button;
        m_actionText = expandedText;
        if (m_actionButton) {
            m_actionButton->setProperty("sidebarAction", true);
            m_actionButton->setToolTip(expandedText);
            if (auto *abstractButton = qobject_cast<QAbstractButton *>(m_actionButton))
                abstractButton->setText(QString());
        }
    }

    bool isExpanded() const { return property("sidebarExpanded").toBool(); }

    void updateItemText(int row, const QString &text)
    {
        if (!m_navigation || row < 0 || row >= m_navigation->count()) return;
        auto *item = m_navigation->item(row);
        item->setData(FullTextRole, text);
        item->setToolTip(text);
        if (isExpanded()) item->setText(text);
    }

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override
#else
    void enterEvent(QEvent *event) override
#endif
    {
        QWidget::enterEvent(event);
        setExpanded(true);
    }

    void leaveEvent(QEvent *event) override
    {
        QWidget::leaveEvent(event);
        setExpanded(false);
    }

private:
    void setExpanded(bool expanded)
    {
        if (isExpanded() == expanded && m_widthAnimation->state() != QAbstractAnimation::Running)
            return;
        setProperty("sidebarExpanded", expanded);
        if (m_navigation) {
            m_navigation->setProperty("sidebarExpanded", expanded);
            for (int row = 0; row < m_navigation->count(); ++row) {
                auto *item = m_navigation->item(row);
                item->setText(expanded ? item->data(FullTextRole).toString() : QString());
            }
        }
        for (const QPointer<QWidget> &widget : m_expandedOnly)
            if (widget) widget->setVisible(expanded);
        if (auto *button = qobject_cast<QAbstractButton *>(m_actionButton.data()))
            button->setText(expanded ? m_actionText : QString());

        style()->unpolish(this);
        style()->polish(this);
        updateGeometry();

        m_widthAnimation->stop();
        const int target = expanded ? m_expandedWidth : m_collapsedWidth;
        m_minimumAnimation->setStartValue(width());
        m_minimumAnimation->setEndValue(target);
        m_maximumAnimation->setStartValue(width());
        m_maximumAnimation->setEndValue(target);
        m_widthAnimation->start();
    }

    int m_collapsedWidth = 68;
    int m_expandedWidth = 180;
    QListWidget *m_navigation = nullptr;
    QList<QPointer<QWidget>> m_expandedOnly;
    QPointer<QWidget> m_actionButton;
    QString m_actionText;
    QParallelAnimationGroup *m_widthAnimation = nullptr;
    QPropertyAnimation *m_minimumAnimation = nullptr;
    QPropertyAnimation *m_maximumAnimation = nullptr;
};

#endif // HOVERSIDEBAR_H
