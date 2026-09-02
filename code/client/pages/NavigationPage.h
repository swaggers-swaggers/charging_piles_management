#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include "types.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;

// 一键导航页: 自绘模拟地图(站点散点 + 当前位置 + 路线) + 驾车/步行方式 + 预计时长距离
// 不依赖外网, 坐标与站点数据来自服务端
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

private:
    QComboBox *m_destCombo;
    QComboBox *m_modeCombo;
    QLabel *m_resultLabel;
    MapCanvas *m_canvas;

    QList<StationInfo> m_stations;
    double m_lon = 123.4500;
    double m_lat = 41.7000;
    int m_destIndex = -1;
};

// 自绘地图控件: 经纬度按边界映射到画布坐标
class MapCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit MapCanvas(QWidget *parent = nullptr);

    void setData(const QList<StationInfo> &stations, double curLon, double curLat,
                 int destIndex);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<StationInfo> m_stations;
    double m_curLon = 0, m_curLat = 0;
    int m_destIndex = -1;
};

#endif // NAVIGATIONPAGE_H
