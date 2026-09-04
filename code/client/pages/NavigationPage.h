#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include "types.h"

#include <QList>
#include <QPair>
#include <QWidget>

class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
#ifdef CHARGING_HAS_WEBENGINE
class QWebEngineView;
#endif

// 一键导航页: 应用内开放地图预览 + 高德 URI 外部导航。
// 不再申请地图 Key；当前地点由用户选择，避免把桌面端 IP 定位误当成 GPS。
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
    void onStartChanged(int index);
    void onPlanChanged();
    void onPreviewRoute();
    void onOpenExternalNavigation();
    void onRouteReplyFinished();

private:
    QComboBox *m_startCombo;
    QComboBox *m_destCombo;
    QComboBox *m_modeCombo;
    QLabel *m_resultLabel;
    QPushButton *m_previewButton;
    QPushButton *m_externalButton;
    MapCanvas *m_canvas;
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_routeReply = nullptr;
    qint64 m_lastRouteRequestAt = 0;

    QList<StationInfo> m_stations;
    double m_lon = 116.3100;
    double m_lat = 39.9600;
    int m_destIndex = -1;
    QList<QPair<double, double>> m_routePolyline;   // WGS-84，(纬度, 经度)
};

// 有 Qt WebEngine 时显示 MapLibre/OpenFreeMap；缺少模块时保留自绘降级画布，
// 使服务端和客户端其他功能仍可构建运行。
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
    QList<QPair<double, double>> m_route;   // WGS-84，(纬度, 经度)

#ifdef CHARGING_HAS_WEBENGINE
    QWebEngineView *m_webView = nullptr;
    bool m_webReady = false;
    QString m_pendingScript;
    void updateWebMap(bool fitView);
#endif

    // 以下状态仅供无 WebEngine 时的降级画布使用。
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
