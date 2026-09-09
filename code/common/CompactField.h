#ifndef COMPACTFIELD_H
#define COMPACTFIELD_H

#include "IconFactory.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QMouseEvent>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QStyle>
#include <QTimer>

// 搜索框和下拉框的统一“按钮态”。默认仅占一个小按钮的位置，点击后再
// 展开内容；失去焦点后自动收起，同时保留查询文字和当前选择。
class CompactFieldController : public QObject
{
public:
    static bool isSearchField(const QLineEdit *edit)
    {
        if (!edit) return false;
        const QString description = edit->objectName() + QLatin1Char(' ')
            + edit->placeholderText() + QLatin1Char(' ') + edit->accessibleName();
        return description.contains(QStringLiteral("搜索"))
            || description.contains(QStringLiteral("查找"))
            || description.contains(QStringLiteral("筛选"))
            || edit->property("compactSearchField").toBool();
    }

    static void install(QWidget *widget)
    {
        if (!widget || widget->property("compactFieldInstalled").toBool()) return;
        auto *combo = qobject_cast<QComboBox *>(widget);
        auto *edit = qobject_cast<QLineEdit *>(widget);
        if (!combo && !isSearchField(edit)) return;
        widget->setProperty("compactFieldInstalled", true);
        new CompactFieldController(widget, combo, edit);
    }

    // 点击另一个控件时关闭已经展开的紧凑输入框。下拉弹窗是独立窗口，
    // 仍被视作所属下拉框的一部分，避免选择条目之前被提前收起。
    static void handleGlobalPress(QWidget *clicked)
    {
        if (!clicked) return;
        auto &items = controllers();
        for (int i = items.size() - 1; i >= 0; --i) {
            auto *controller = items.at(i).data();
            if (!controller) {
                items.removeAt(i);
                continue;
            }
            QWidget *field = controller->m_field.data();
            if (!field || !controller->isExpanded()) continue;
            if (clicked == field || field->isAncestorOf(clicked)) continue;
            if (controller->m_combo && controller->m_combo->view()) {
                QWidget *view = controller->m_combo->view();
                if (clicked == view || view->isAncestorOf(clicked)) continue;
            }
            controller->setExpanded(false);
        }
    }

private:
    CompactFieldController(QWidget *field, QComboBox *combo, QLineEdit *edit)
        : QObject(field), m_field(field), m_combo(combo), m_edit(edit)
    {
        controllers().append(this);
        const int naturalWidth = qMax(field->minimumWidth(), field->sizeHint().width());
        m_expandedWidth = field->property("compactExpandedWidth").isValid()
            ? field->property("compactExpandedWidth").toInt()
            : edit ? qBound(240, naturalWidth, 340) : qBound(170, naturalWidth + 24, 280);
        if (m_edit) {
            m_placeholder = m_edit->placeholderText();
            m_clearButtonEnabled = m_edit->isClearButtonEnabled();
            m_edit->addAction(IconFactory::navigationIcon(IconFactory::IconSearch),
                              QLineEdit::LeadingPosition);
        }
        if (m_combo) {
            m_combo->setToolTip(m_combo->currentText());
            connect(m_combo, &QComboBox::currentTextChanged, this,
                    [this](const QString &text) { if (!isExpanded()) m_combo->setToolTip(text); });
            connect(m_combo, QOverload<int>::of(&QComboBox::activated), this,
                    [this](int) { QTimer::singleShot(80, this, [this] { setExpanded(false); }); });
        }

        m_group = new QParallelAnimationGroup(this);
        m_minimum = new QPropertyAnimation(field, "minimumWidth", m_group);
        m_maximum = new QPropertyAnimation(field, "maximumWidth", m_group);
        for (auto *animation : {m_minimum, m_maximum}) {
            animation->setDuration(190);
            animation->setEasingCurve(QEasingCurve::OutCubic);
            m_group->addAnimation(animation);
        }
        field->installEventFilter(this);
        field->setCursor(Qt::PointingHandCursor);
        applyState(false);
        field->setMinimumWidth(m_compactWidth);
        field->setMaximumWidth(m_compactWidth);
    }

    ~CompactFieldController() override
    {
        controllers().removeAll(this);
    }

    static QList<QPointer<CompactFieldController>> &controllers()
    {
        static QList<QPointer<CompactFieldController>> items;
        return items;
    }

    bool isExpanded() const { return m_field->property("compactExpanded").toBool(); }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object != m_field) return false;
        if (event->type() == QEvent::MouseButtonPress && !isExpanded()) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                setExpanded(true);
                if (m_combo) {
                    QTimer::singleShot(210, m_combo, [combo = m_combo] {
                        if (!combo) return;
                        combo->setFocus(Qt::MouseFocusReason);
                        combo->showPopup();
                    });
                    return true;
                }
                if (m_edit)
                    m_edit->setFocus(Qt::MouseFocusReason);
            }
        } else if (event->type() == QEvent::FocusOut) {
            QTimer::singleShot(140, this, [this] {
                if (!m_field || m_field->hasFocus()) return;
                if (m_combo && m_combo->view() && m_combo->view()->isVisible()) return;
                setExpanded(false);
            });
        } else if (event->type() == QEvent::KeyPress && isExpanded()) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                if (m_combo) m_combo->hidePopup();
                m_field->clearFocus();
                setExpanded(false);
                return true;
            }
        } else if (event->type() == QEvent::Hide) {
            setExpanded(false, false);
        }
        return false;
    }

    void setExpanded(bool expanded, bool animated = true)
    {
        if (!m_field || isExpanded() == expanded) return;
        applyState(expanded);
        if (!animated) {
            m_field->setMinimumWidth(expanded ? m_expandedWidth : m_compactWidth);
            m_field->setMaximumWidth(expanded ? m_expandedWidth : m_compactWidth);
            return;
        }
        m_group->stop();
        const int target = expanded ? m_expandedWidth : m_compactWidth;
        m_minimum->setStartValue(m_field->width());
        m_minimum->setEndValue(target);
        m_maximum->setStartValue(m_field->width());
        m_maximum->setEndValue(target);
        m_group->start();
    }

    void applyState(bool expanded)
    {
        m_field->setProperty("compactExpanded", expanded);
        if (m_edit) {
            m_edit->setPlaceholderText(expanded ? m_placeholder : QString());
            m_edit->setClearButtonEnabled(expanded && m_clearButtonEnabled);
            m_edit->setCursor(expanded ? Qt::IBeamCursor : Qt::PointingHandCursor);
            m_edit->setToolTip(expanded ? QString()
                                        : (m_edit->text().isEmpty() ? m_placeholder : m_edit->text()));
        }
        if (m_combo)
            m_combo->setToolTip(expanded ? QString() : m_combo->currentText());
        m_field->style()->unpolish(m_field);
        m_field->style()->polish(m_field);
        m_field->update();
    }

    QPointer<QWidget> m_field;
    QPointer<QComboBox> m_combo;
    QPointer<QLineEdit> m_edit;
    QString m_placeholder;
    bool m_clearButtonEnabled = false;
    int m_compactWidth = 42;
    int m_expandedWidth = 260;
    QParallelAnimationGroup *m_group = nullptr;
    QPropertyAnimation *m_minimum = nullptr;
    QPropertyAnimation *m_maximum = nullptr;
};

#endif // COMPACTFIELD_H
