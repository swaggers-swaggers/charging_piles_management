#ifndef STATUSBADGE_H
#define STATUSBADGE_H

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>

// 三态小徽章：空闲为绿色，充电中为蓝色呼吸，故障为红色。
class StatusBadge : public QLabel
{
public:
    enum Kind { Idle, Charging, Fault };

    explicit StatusBadge(Kind kind, QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setObjectName(QStringLiteral("statusBadge"));
        setAlignment(Qt::AlignCenter);
        setMinimumHeight(26);
        setKind(kind);
    }

    void setKind(Kind kind)
    {
        m_kind = kind;
        if (m_animation) {
            m_animation->stop();
            delete m_animation;
            m_animation = nullptr;
        }
        setGraphicsEffect(nullptr);

        if (kind == Idle) {
            setText(QStringLiteral("●  空闲"));
            setStyleSheet(QStringLiteral(
                "QLabel#statusBadge{color:#176B47;background:#E4F5EA;border:1px solid #C4E6D0;"
                "border-radius:12px;padding:3px 10px;font-weight:650;}"));
        } else if (kind == Fault) {
            setText(QStringLiteral("●  故障"));
            setStyleSheet(QStringLiteral(
                "QLabel#statusBadge{color:#B33F4B;background:#FCE8EB;border:1px solid #F0C4CA;"
                "border-radius:12px;padding:3px 10px;font-weight:650;}"));
        } else {
            setText(QStringLiteral("●  充电中"));
            setStyleSheet(QStringLiteral(
                "QLabel#statusBadge{color:#155FBE;background:#E2EEFF;border:1px solid #9EC2F3;"
                "border-radius:12px;padding:3px 10px;font-weight:650;}"));
            auto *effect = new QGraphicsOpacityEffect(this);
            effect->setOpacity(1.0);
            setGraphicsEffect(effect);
            m_animation = new QPropertyAnimation(effect, "opacity", this);
            m_animation->setObjectName(QStringLiteral("chargingBadgeBreath"));
            m_animation->setDuration(1050);
            m_animation->setStartValue(1.0);
            m_animation->setEndValue(0.58);
            m_animation->setEasingCurve(QEasingCurve::InOutSine);
            m_animation->setLoopCount(-1);
            m_animation->start();
        }
    }

    Kind kind() const { return m_kind; }

private:
    Kind m_kind = Idle;
    QPropertyAnimation *m_animation = nullptr;
};

#endif // STATUSBADGE_H
