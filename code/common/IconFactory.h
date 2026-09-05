#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QRectF>

// 纯代码绘制的线性矢量图标: 不依赖系统 emoji 字体, 也不依赖外部图片文件,
// 客户端/服务端共用, 在任何平台(Linux/Windows)显示完全一致。
// 统一在 24x24 逻辑网格上绘制, 再按 px 缩放, 高分屏也清晰。
class IconFactory
{
public:
    enum IconType {
        IconBolt = 0,      // 闪电: 充电动作 / Logo
        IconChartLine,     // 折线图: 销售业绩
        IconBattery,       // 电池: 电桩状态
        IconPile,          // 充电桩: 充电桩管理
        IconBuilding,      // 楼宇: 充电站管理
        IconUsers,         // 多人: 用户管理
        IconUser,          // 单人: 用户信息
        IconLocation,      // 定位针: 附近充电站
        IconCompass,       // 罗盘: 一键导航
    };

    static QIcon icon(IconType type, const QColor &color = QColor("#D8E4F0"), int px = 48)
    {
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.scale(px / 24.0, px / 24.0);
        QPen pen(color, 1.7);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        draw(type, &painter, color);
        painter.end();
        return QIcon(pm);
    }

private:
    static void draw(IconType type, QPainter *p, const QColor &c)
    {
        switch (type) {
        case IconBolt: {
            QPolygonF poly;
            poly << QPointF(13, 2) << QPointF(5.5, 13) << QPointF(10.5, 13)
                 << QPointF(11, 22) << QPointF(18.5, 10.5) << QPointF(13.2, 10.5);
            p->setBrush(c);
            p->drawPolygon(poly);
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconChartLine: {
            p->drawLine(4, 4, 4, 20);                 // 纵轴
            p->drawLine(4, 20, 21, 20);               // 横轴
            QPainterPath path;
            path.moveTo(6.5, 15.5);
            path.lineTo(10, 11.5);
            path.lineTo(13, 13.5);
            path.lineTo(18, 7);
            p->drawPath(path);                        // 趋势折线
            p->setBrush(c);
            const QPointF pts[4] = {{6.5, 15.5}, {10, 11.5}, {13, 13.5}, {18, 7}};
            for (const QPointF &pt : pts)
                p->drawEllipse(pt, 1.3, 1.3);         // 数据点
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconBattery: {
            p->drawRoundedRect(QRectF(3, 8, 16, 8), 2, 2);
            p->setBrush(c);
            p->drawRect(QRectF(19.3, 10.8, 1.9, 2.4));   // 正极帽
            p->drawRoundedRect(QRectF(5, 10, 7.5, 4), 1, 1); // 内部电量
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconPile: {
            p->drawRoundedRect(QRectF(7, 2.5, 10, 19), 2.2, 2.2); // 桩体
            p->drawRect(QRectF(9.5, 5.5, 5, 3.6));               // 显示屏
            p->drawLine(9.5, 14, 14.5, 14);                       // 分隔线
            QPainterPath hose;                                     // 充电枪线
            hose.moveTo(17, 15);
            hose.cubicTo(20.5, 15, 20.5, 19, 17.5, 19);
            p->drawPath(hose);
            p->drawLine(17.5, 19, 17.5, 21.2);                    // 枪头
            break;
        }
        case IconBuilding: {
            p->drawRoundedRect(QRectF(4.5, 4, 9.5, 16), 1.2, 1.2); // 主楼
            p->drawRect(QRectF(14, 10, 5.5, 10));                  // 副楼
            p->setBrush(c);
            for (int r = 0; r < 3; ++r)
                for (int col = 0; col < 2; ++col)
                    p->drawRect(QRectF(6.6 + col * 3.2, 7 + r * 3.4, 1.6, 1.6)); // 主楼窗
            p->drawRect(QRectF(15.6, 12.5, 2.3, 1.8));
            p->drawRect(QRectF(15.6, 16, 2.3, 1.8));
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconUsers: {
            p->drawEllipse(QPointF(15.5, 8.5), 2.2, 2.2);          // 后一人头
            p->drawArc(QRectF(12, 12.5, 7, 5.5), 0, 180 * 16);     // 后一人肩
            p->drawEllipse(QPointF(9, 9), 2.8, 2.8);               // 前一人头
            p->drawArc(QRectF(4.5, 13, 9, 7), 0, 180 * 16);        // 前一人肩
            break;
        }
        case IconUser: {
            p->drawEllipse(QPointF(12, 8.5), 3, 3);
            p->drawArc(QRectF(5.5, 13, 13, 8.5), 0, 180 * 16);
            break;
        }
        case IconLocation: {
            p->drawEllipse(QPointF(12, 9.5), 5, 5);
            QPolygonF tri;
            tri << QPointF(12, 21) << QPointF(8.3, 13.4) << QPointF(15.7, 13.4);
            p->drawPolygon(tri);
            p->setBrush(c);
            p->drawEllipse(QPointF(12, 9.5), 1.8, 1.8);
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconCompass: {
            p->drawEllipse(QRectF(3, 3, 18, 18));
            QPolygonF needle;
            needle << QPointF(12, 6.5) << QPointF(14.5, 12) << QPointF(12, 17.5) << QPointF(9.5, 12);
            p->setBrush(c);
            p->drawPolygon(needle);
            p->setBrush(Qt::NoBrush);
            break;
        }
        }
    }
};

#endif // ICONFACTORY_H
