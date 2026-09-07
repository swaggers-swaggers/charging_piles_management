#ifndef UIMOTION_H
#define UIMOTION_H
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QPushButton>
#include <QStackedWidget>
#include <QEvent>
#include <QApplication>
#include <QAbstractButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QMouseEvent>
#include <QTimer>
#include <cmath>
#include "WindowChrome.h"

namespace UiMotion {
// 仅绘制短暂覆盖层，不改变布局或给 WebEngine 添加图形效果。
class Overlay : public QWidget {
public:
    explicit Overlay(QWidget *target, bool ripple, QPointF origin = QPointF(-1,-1)) : QWidget(target), m_origin(origin.x()<0 ? QPointF(target->rect().center()) : origin), m_ripple(ripple) {
        setObjectName("motionOverlay");
        setProperty("rippleOrigin", m_origin);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(target->rect());
        target->installEventFilter(this);
        m_animation.setDuration(ripple ? 460 : 180);
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
            const qreal radius = std::hypot(qMax(m_origin.x(), width()-m_origin.x()), qMax(m_origin.y(),height()-m_origin.y())) * m_progress;
            p.drawEllipse(m_origin, radius, radius);
        } else {
            QColor color("#F3F7F6"); color.setAlphaF(.65 * (1 - m_progress));
            p.fillRect(rect(), color);
        }
    }
private:
    QVariantAnimation m_animation;
    qreal m_progress = 0;
    QPointF m_origin;
    bool m_ripple;
};
inline void play(QWidget *target, bool ripple = false) {
    if (!target->isVisible()) return;
    for (auto *old : target->findChildren<QWidget*>("motionOverlay", Qt::FindDirectChildrenOnly)) delete old;
    new Overlay(target, ripple);
}
// Shared timing: hover 180 ms, ripple 460 ms, page 180 ms; all ease out.
// Focus uses a symmetric sine cycle, with zero velocity at each turning point.
class Highlight : public QWidget {
public:
    Highlight(QWidget *target, bool focus) : QWidget(target), focused(focus) {
        setObjectName(focus ? "focusBreathingOverlay" : "hoverOverlay");
        setAttribute(Qt::WA_TransparentForMouseEvents); setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(target->rect()); target->installEventFilter(this);
        animation.setDuration(focus ? 1800 : 180);
        animation.setStartValue(0.0); animation.setEndValue(1.0);
        animation.setEasingCurve(focus ? QEasingCurve::InOutSine : QEasingCurve::OutCubic);
        if(focus) { animation.setKeyValueAt(.5,1.0); animation.setEndValue(0.0); animation.setLoopCount(-1); }
        connect(&animation,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){level=v.toReal();update();});
        show(); raise(); animation.start();
    }
    void leave() {
        animation.stop(); animation.setDuration(180); animation.setStartValue(level); animation.setEndValue(0.0);
        connect(&animation,&QVariantAnimation::finished,this,&QObject::deleteLater); animation.start();
    }
protected:
    bool eventFilter(QObject *,QEvent *e) override {
        if(e->type()==QEvent::Resize) setGeometry(parentWidget()->rect());
        if(e->type()==QEvent::Hide) {hide();deleteLater();}
        return false;
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        QColor c("#38B987"); c.setAlphaF(focused ? .45+.45*level : .12*level);
        p.setPen(focused ? QPen(c,2) : QPen(Qt::NoPen));
        p.setBrush(focused ? Qt::NoBrush : QBrush(c));
        p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),7,7);
    }
private:
    QVariantAnimation animation; bool focused; qreal level=0;
};
class InteractionFilter : public QObject {
public:
    explicit InteractionFilter(QObject *p) : QObject(p) {}
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        auto *w=qobject_cast<QWidget*>(object);
        if(!w || w->testAttribute(Qt::WA_TransparentForMouseEvents)) return false;
        const bool clickable=qobject_cast<QAbstractButton*>(w) || w->property("clickableCard").toBool()
            || (w->testAttribute(Qt::WA_SetCursor) && w->cursor().shape()==Qt::PointingHandCursor);
        // Install before show/exec: changing flags after a modal dialog is visible
        // hides it and prematurely exits its nested event loop.
        if(event->type()==QEvent::Polish && w->isWindow()) installChrome(w);
        if(event->type()==QEvent::Show) {
            if(auto *stack=qobject_cast<QStackedWidget*>(w)) {
                if(!stack->property("motionInstalled").toBool()) {
                    stack->setProperty("motionInstalled",true);
                    connect(stack,&QStackedWidget::currentChanged,stack,[stack]{play(stack);});
                }
            }
        }
        if(event->type()==QEvent::EnabledChange && !w->isEnabled()) {
            qDeleteAll(w->findChildren<QWidget*>("hoverOverlay",Qt::FindDirectChildrenOnly));
            qDeleteAll(w->findChildren<QWidget*>("focusBreathingOverlay",Qt::FindDirectChildrenOnly));
        }
        if(clickable && w->isEnabled()) {
            if(event->type()==QEvent::MouseButtonPress) {
                auto *mouse=static_cast<QMouseEvent*>(event);
                if(mouse->button()==Qt::LeftButton) {
                    auto old=w->findChildren<QWidget*>("motionOverlay",Qt::FindDirectChildrenOnly);
                    if(old.size()>=4) delete old.first();
                    new Overlay(w,true,mouse->pos());
                }
            } else if(event->type()==QEvent::Enter) {
                qDeleteAll(w->findChildren<QWidget*>("hoverOverlay",Qt::FindDirectChildrenOnly));
                new Highlight(w,false);
            } else if(event->type()==QEvent::Leave || event->type()==QEvent::EnabledChange) {
                for(auto *h:w->findChildren<QWidget*>("hoverOverlay",Qt::FindDirectChildrenOnly)) static_cast<Highlight*>(h)->leave();
            }
        }
        const bool input=qobject_cast<QLineEdit*>(w)||qobject_cast<QTextEdit*>(w)||qobject_cast<QPlainTextEdit*>(w)
            ||qobject_cast<QComboBox*>(w)||qobject_cast<QAbstractSpinBox*>(w);
        if(input && event->type()==QEvent::FocusIn) {
            qDeleteAll(w->findChildren<QWidget*>("focusBreathingOverlay",Qt::FindDirectChildrenOnly));
            new Highlight(w,true);
        }
        if(input && (event->type()==QEvent::FocusOut || event->type()==QEvent::Hide))
            qDeleteAll(w->findChildren<QWidget*>("focusBreathingOverlay",Qt::FindDirectChildrenOnly));
        return false;
    }
};
inline void install(QWidget *root) {
    if(!qApp->property("interactionFilterInstalled").toBool()) {
        qApp->setProperty("interactionFilterInstalled",true);
        qApp->installEventFilter(new InteractionFilter(qApp));
    }
    installChrome(root);
    for(auto *stack:root->findChildren<QStackedWidget*>()) {
        if(stack->property("motionInstalled").toBool()) continue;
        stack->setProperty("motionInstalled",true);
        QObject::connect(stack,&QStackedWidget::currentChanged,stack,[stack]{play(stack);});
    }
}
}
#endif
