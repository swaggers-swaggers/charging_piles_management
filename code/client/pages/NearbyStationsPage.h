#ifndef NEARBYSTATIONSPAGE_H
#define NEARBYSTATIONSPAGE_H

#include "types.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QVBoxLayout;
class QLabel;
class QCheckBox;

// 附近充电站查询页(项目说明书):
//   定位: 通过下拉框选择区域，不依赖第三方地图 Key
//   列表: 按距离由近及远展示, 点击查看该站所有电桩的详细信息
class NearbyStationsPage : public QWidget
{
    Q_OBJECT

public:
    explicit NearbyStationsPage(QWidget *parent = nullptr);

    // 供导航页复用最新站点数据(含坐标与距离)
    QList<StationInfo> stations() const { return m_stations; }
    double currentLon() const { return m_lon; }
    double currentLat() const { return m_lat; }

public slots:
    void refreshPage();

signals:
    void chargeRequested(int stationId);
    void navigationRequested(int stationId, double lon, double lat);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void refresh();
    void onRegionChanged(int index);
    void renderStations();
    void showPileDetail(int stationId);

private:
    QComboBox *m_regionCombo;
    QVBoxLayout *m_cards;
    QLabel *m_summary;
    QLabel *m_count;
    QLineEdit *m_search;
    QCheckBox *m_idleOnly;
    QComboBox *m_sort;
    bool m_refreshing = false;

    QList<StationInfo> m_stations;
    double m_lon = 116.3100;
    double m_lat = 39.9600;
};

#endif // NEARBYSTATIONSPAGE_H
