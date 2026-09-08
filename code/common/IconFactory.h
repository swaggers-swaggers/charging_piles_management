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
        IconPlug,          // 插头+闪电: 附近充电站顶部艺术图标(替代外部 svg, 免依赖 QtSvg)
        IconHome,          // 首页: Bento 功能总览
        IconCar,           // 汽车: 我的车辆
        IconWallet,        // 钱包: 历史消费
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
        case IconPlug: {
            // 插头本体 + 两个引脚
            p->drawRoundedRect(QRectF(3, 13, 6, 7.5), 1.2, 1.2);
            p->drawRect(QRectF(4.8, 8.2, 1.7, 5));
            p->drawRect(QRectF(7.8, 8.2, 1.7, 5));
            p->drawLine(3, 20.5, 2, 22);       // 电线钩
            // 右侧闪电(实心)
            QPolygonF zap;
            zap << QPointF(15.5, 2.5) << QPointF(10.3, 12.5) << QPointF(13.7, 12.5)
                 << QPointF(14.1, 21.5) << QPointF(20, 10.5) << QPointF(15.7, 10.5);
            p->setBrush(c);
            p->drawPolygon(zap);
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconHome: {
            QPainterPath roof;
            roof.moveTo(3.5, 11.2);
            roof.lineTo(12, 3.8);
            roof.lineTo(20.5, 11.2);
            p->drawPath(roof);
            p->drawRoundedRect(QRectF(5.5, 10, 13, 10.5), 1.6, 1.6);
            p->drawRoundedRect(QRectF(10, 14, 4, 6.5), 1, 1);
            break;
        }
        case IconCar: {
            // 汽车侧面剪影: 车身 + 车窗 + 两轮
            QPainterPath body;
            body.moveTo(4, 15);
            body.lineTo(4.5, 12);
            body.quadTo(7, 8.5, 11, 8.5);
            body.lineTo(14.5, 8.5);
            body.quadTo(18, 8.5, 20, 12);
            body.lineTo(20.5, 15);
            body.quadTo(20.5, 17, 18.5, 17);
            body.lineTo(5.5, 17);
            body.quadTo(3.5, 17, 4, 15);
            p->drawPath(body);
            p->drawLine(11, 12, 14.5, 12);            // 车窗分隔
            p->drawLine(12.8, 9.8, 12.8, 12);         // 车窗立柱
            p->setBrush(c);
            p->drawEllipse(QPointF(8, 17), 2, 2);     // 前轮
            p->drawEllipse(QPointF(16.4, 17), 2, 2);  // 后轮
            p->setBrush(Qt::NoBrush);
            break;
        }
        case IconWallet: {
            // 钱包: 主体 + 翻盖 + 圆形硬币
            p->drawRoundedRect(QRectF(3.5, 6, 17, 13), 2, 2);
            p->drawRoundedRect(QRectF(3.5, 6, 17, 4.5), 2, 2);
            p->drawEllipse(QPointF(12, 15.5), 2.6, 2.6);   // 硬币
            p->drawLine(4.5, 6.5, 4.5, 18.5);               // 左缝线
            break;
        }
        }
    }
};

#endif // ICONFACTORY_H
