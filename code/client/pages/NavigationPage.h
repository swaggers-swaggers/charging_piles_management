#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include "types.h"

#include <QList>
#include <QPair>
#include <QWidget>

class QComboBox;
class QLabel;

// 一键导航页: 自绘模拟地图(站点散点 + 当前位置 + 路线) + 驾车/步行方式 + 预计时长距离
// 不依赖外网, 坐标与站点数据来自服务端; 当前位置可用腾讯 IP 定位获取(无 GPS 时的近似方案)
class MapCanvas;

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refresh();
    void onPlanChanged();
    void onNavigate();
    void onLocate();

private:
    QComboBox *m_destCombo;
    QComboBox *m_modeCombo;
    QLabel *m_resultLabel;
    MapCanvas *m_canvas;

    QList<StationInfo> m_stations;
    double m_lon = 116.3100;   // 默认北京海淀区(与附近充电站页一致)
    double m_lat = 39.9600;
    int m_destIndex = -1;
    bool m_autoLocated = false;
    QList<QPair<double, double>> m_routePolyline;   // (纬度, 经度) 真实路线折线
};

// 自绘地图控件: 经纬度按边界映射到画布坐标, 支持滚轮/按钮缩放
class MapCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit MapCanvas(QWidget *parent = nullptr);

    void setData(const QList<StationInfo> &stations, double curLon, double curLat,
                 int destIndex, const QList<QPair<double, double>> &routePolyline);

    void zoomIn();    // 放大
    void zoomOut();   // 缩小

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QList<StationInfo> m_stations;
    double m_curLon = 0, m_curLat = 0;
    int m_destIndex = -1;
    QList<QPair<double, double>> m_route;   // (纬度, 经度)

    double m_zoom = 1.0;                          // 缩放倍率, 1.0 = 全览
    double m_centerLon = 0, m_centerLat = 0;      // 当前视野中心(缩放时可移动)
    double m_baseCenterLon = 0, m_baseCenterLat = 0;  // 全览时的中心
    double m_baseSpanLon = 0.01, m_baseSpanLat = 0.01; // 全览时的跨度

    bool m_dragging = false;                      // 是否正在拖拽
    QPoint m_dragStart;                           // 拖拽起始像素位置
    double m_dragStartCenterLon = 0, m_dragStartCenterLat = 0;  // 拖拽起始视野中心

    void updateBaseBounds();
    void zoomAt(const QPointF &cursorPos, double factor);
};

#endif // NAVIGATIONPAGE_H
