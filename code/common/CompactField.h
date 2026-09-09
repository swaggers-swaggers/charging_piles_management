#ifndef COMPACTFIELD_H
#define COMPACTFIELD_H

#include "IconFactory.h"

#include <QComboBox>
#include <QLineEdit>
#include <QStyle>

// 搜索框和下拉框的统一常驻展开样式：始终显示搜索提示或当前选项，
// 点击、选择和失去焦点时都不改变控件宽度。
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

        const int naturalWidth = qMax(widget->minimumWidth(), widget->sizeHint().width());
        const int expandedWidth = widget->property("compactExpandedWidth").isValid()
            ? widget->property("compactExpandedWidth").toInt()
            : edit ? qBound(240, naturalWidth, 340)
                   : qBound(170, naturalWidth + 24, 280);

        if (edit) {
            edit->addAction(IconFactory::navigationIcon(IconFactory::IconSearch),
                            QLineEdit::LeadingPosition);
            edit->setCursor(Qt::IBeamCursor);
        }
        widget->setToolTip(QString());
        widget->setProperty("compactExpanded", true);
        widget->setMinimumWidth(expandedWidth);
        if (widget->maximumWidth() < expandedWidth)
            widget->setMaximumWidth(expandedWidth);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    // 保留统一交互过滤器的调用接口；常驻展开后，外部点击无需处理。
    static void handleGlobalPress(QWidget *) {}
};

#endif // COMPACTFIELD_H
