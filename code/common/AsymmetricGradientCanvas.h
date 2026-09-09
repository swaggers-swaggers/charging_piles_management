#ifndef ASYMMETRICGRADIENTCANVAS_H
#define ASYMMETRICGRADIENTCANVAS_H

#include <QPainter>
#include <QPainterPath>
#include <QWidget>

// 主窗口内容画布：颜色必须先过渡到白色，再进入下一种颜色。
// 不等距的色标避免背景形成机械、对称的色带；边缘高光由 paintEvent 直接绘制。
class AsymmetricGradientCanvas : public QWidget
{
public:
    explicit AsymmetricGradientCanvas(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("appCentral"));
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath mask;
        mask.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 18, 18);
        painter.setClipPath(mask);

        QLinearGradient background(QPointF(width() * 0.03, height() * 0.08),
                                   QPointF(width() * 0.91, height() * 0.96));
        background.setColorAt(0.00, QColor("#8EA487"));
        background.setColorAt(0.17, QColor("#FFFFFF"));
        background.setColorAt(0.43, QColor("#DDD5B9"));
        background.setColorAt(0.68, QColor("#FFFFFF"));
        background.setColorAt(1.00, QColor("#5E8A58"));
        painter.fillPath(mask, background);

        // 用裁剪遮罩把高光限定在主页面边缘，右下角刻意比左上角更弱。
        QLinearGradient edge(QPointF(0, 0), QPointF(width(), height()));
        edge.setColorAt(0.00, QColor(255, 255, 255, 238));
        edge.setColorAt(0.24, QColor(220, 255, 232, 188));
        edge.setColorAt(0.61, QColor(255, 255, 255, 92));
        edge.setColorAt(1.00, QColor(126, 195, 145, 132));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QBrush(edge), 3.0));
        painter.drawRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), 17, 17);

        QLinearGradient innerEdge(QPointF(width(), 0), QPointF(0, height()));
        innerEdge.setColorAt(0.00, QColor(255, 255, 255, 170));
        innerEdge.setColorAt(0.37, QColor(255, 255, 255, 35));
        innerEdge.setColorAt(1.00, QColor(181, 232, 194, 105));
        painter.setPen(QPen(QBrush(innerEdge), 1.0));
        painter.drawRoundedRect(QRectF(rect()).adjusted(6, 6, -6, -6), 14, 14);
    }
};

#endif // ASYMMETRICGRADIENTCANVAS_H
