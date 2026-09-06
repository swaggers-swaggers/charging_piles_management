#ifndef UIMOTION_H
#define UIMOTION_H
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QPushButton>
#include <QStackedWidget>
#include <QEvent>

namespace UiMotion {
// 仅绘制短暂覆盖层，不改变布局或给 WebEngine 添加图形效果。
class Overlay : public QWidget {
public:
    explicit Overlay(QWidget *target, bool ripple) : QWidget(target), m_ripple(ripple) {
        setObjectName("motionOverlay");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(target->rect());
        target->installEventFilter(this);
        m_animation.setDuration(ripple ? 240 : 180);
        m_animation.setStartValue(0.0);
        m_animation.setEndValue(1.0);
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_progress = v.toReal(); update();
        });
        connect(&m_animation, &QVariantAnimation::finished, this, &QObject::deleteLater);
        show(); raise(); m_animation.start();
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Resize) setGeometry(parentWidget()->rect());
        return false;
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        if (m_ripple) {
            QPainterPath clip; clip.addRoundedRect(QRectF(rect()), 7, 7); p.setClipPath(clip);
            QColor color("#60BB91"); color.setAlphaF(.25 * (1 - m_progress));
            p.setPen(Qt::NoPen); p.setBrush(color);
            const qreal radius = qMax(width(), height()) * m_progress;
            p.drawEllipse(QPointF(rect().center()), radius, radius);
        } else {
            QColor color("#F3F7F6"); color.setAlphaF(.65 * (1 - m_progress));
            p.fillRect(rect(), color);
        }
    }
private:
    QVariantAnimation m_animation;
    qreal m_progress = 0;
    bool m_ripple;
};
inline void play(QWidget *target, bool ripple = false) {
    if (!target->isVisible()) return;
    for (auto *old : target->findChildren<QWidget*>("motionOverlay", Qt::FindDirectChildrenOnly)) delete old;
    new Overlay(target, ripple);
}
inline void install(QWidget *root) {
    for (auto *button : root->findChildren<QPushButton*>()) {
        if (button->property("motionInstalled").toBool()) continue;
        button->setProperty("motionInstalled", true);
        QObject::connect(button, &QPushButton::pressed, button, [button] { play(button, true); });
    }
    for (auto *stack : root->findChildren<QStackedWidget*>()) {
        if (stack->property("motionInstalled").toBool()) continue;
        stack->setProperty("motionInstalled", true);
        QObject::connect(stack, &QStackedWidget::currentChanged, stack, [stack] { play(stack); });
    }
}
}
#endif
