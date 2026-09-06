#ifndef DONUTCHART_H
#define DONUTCHART_H
#include <QWidget>
#include <QPainter>
#include <QVariantAnimation>
#include <QShowEvent>

class DonutChart : public QWidget {
public:
    DonutChart(int inUse,int idle,int fault,QWidget *parent=nullptr)
        : QWidget(parent), m_values{qMax(0,inUse),qMax(0,idle),qMax(0,fault)} {
        setObjectName("statusDonut"); setMinimumSize(220,280);
        m_reveal.setDuration(420); m_reveal.setStartValue(0.0); m_reveal.setEndValue(1.0);
        m_reveal.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_reveal,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){m_progress=v.toReal();update();});
    }
    QRectF ringRect() const {
        const QRectF area(24,42,qMax(0,width()-48),qMax(0,height()-142));
        const qreal side=qMin(area.width(),area.height());
        return QRectF(area.center().x()-side/2,area.center().y()-side/2,side,side);
    }
protected:
    void showEvent(QShowEvent *e) override { QWidget::showEvent(e); m_reveal.start(); }
    void hideEvent(QHideEvent *e) override { m_reveal.stop(); QWidget::hideEvent(e); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const QRectF ring=ringRect(); const qreal thickness=ring.width()*.13;
        const QRectF arc=ring.adjusted(thickness/2,thickness/2,-thickness/2,-thickness/2);
        const int total=m_values[0]+m_values[1]+m_values[2];
        p.setPen(QColor("#294D3D")); QFont font=p.font();font.setPixelSize(14);font.setBold(true);p.setFont(font);
        p.drawText(QRect(16,12,width()-32,24),Qt::AlignCenter,"设备状态分布");
        p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#EDF3EF"),thickness));p.drawEllipse(arc);
        qreal start=90*16;
        for(int i=0;i<3 && total>0;++i) {
            const qreal span=360.0*16*m_values[i]/total*m_progress;
            p.setPen(QPen(m_colors[i],thickness,Qt::SolidLine,Qt::FlatCap));
            p.drawArc(arc,qRound(start),-qRound(span));start-=span;
        }
        p.setPen(QColor("#214B36"));font.setPixelSize(qBound(18,int(ring.width()*.17),34));font.setBold(true);p.setFont(font);
        p.drawText(QRectF(ring.left(),ring.center().y()-26,ring.width(),36),Qt::AlignCenter,QString::number(total));
        font.setPixelSize(12);font.setBold(false);p.setFont(font);p.setPen(QColor("#74887B"));
        p.drawText(QRectF(ring.left(),ring.center().y()+12,ring.width(),22),Qt::AlignCenter,total ? "电桩总数" : "暂无设备");
        const QStringList names{"在用","闲置","故障"};
        for(int i=0;i<3;++i) {
            const int y=height()-80+i*24;
            p.setPen(Qt::NoPen);p.setBrush(m_colors[i]);p.drawEllipse(QPointF(30,y+8),4,4);
            p.setPen(QColor("#576D60"));
            p.drawText(QRect(44,y,width()-60,20),Qt::AlignVCenter,
                QString("%1   %2 台  ·  %3%").arg(names[i]).arg(m_values[i]).arg(total?100.0*m_values[i]/total:0,0,'f',1));
        }
    }
private:
    int m_values[3];
    QColor m_colors[3]{QColor("#CCA052"),QColor("#31A675"),QColor("#D36A73")};
    QVariantAnimation m_reveal;
    qreal m_progress=1;
};
#endif
