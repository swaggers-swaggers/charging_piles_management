#ifndef CHARGINGPARTICLES_H
#define CHARGINGPARTICLES_H
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>
#include <cmath>

// Background belongs to the charging view; hidden pages consume no frame timer.
class ChargingParticles : public QWidget {
public:
    explicit ChargingParticles(QWidget *parent=nullptr) : QWidget(parent) {
        setObjectName("chargingParticleBackground");
        timer.setInterval(33);
        connect(&timer,&QTimer::timeout,this,[this]{update();});
    }
    bool animationRunning() const { return timer.isActive(); }
protected:
    void showEvent(QShowEvent *e) override {clock.start();timer.start();QWidget::showEvent(e);}
    void hideEvent(QHideEvent *e) override {timer.stop();QWidget::hideEvent(e);}
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);
        const double t=clock.elapsed()/1000.0;
        for(int i=0;i<40;++i) {
            const double phase=std::fmod(t/(7.0+i%6)+i*.61803398875,1.0);
            const double eased=.5-.5*std::cos(phase*3.14159265359);
            const double seed=std::sin((i+1)*127.1)*43758.5453;
            const double x=width()*(seed-std::floor(seed)) + 18*std::sin(t*.6+i);
            const double y=height()*(1.08-1.16*eased);
            QColor c(i%3==0 ? "#59B7CC" : "#42BD8C");c.setAlphaF(.28*std::sin(phase*3.14159265359));
            const double radius=2+i%4;
            QRadialGradient glow(QPointF(x,y),radius*4);glow.setColorAt(0,c);c.setAlpha(0);glow.setColorAt(1,c);
            p.setBrush(glow);p.drawEllipse(QPointF(x,y),radius*4,radius*4);
        }
    }
private:
    QTimer timer;QElapsedTimer clock;
};
#endif
