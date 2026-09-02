#ifndef NEARBYSTATIONSPAGE_H
#define NEARBYSTATIONSPAGE_H

#include "types.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QTableWidget;

// 附近充电站查询页: 模拟GPS定位(区域下拉→固定坐标) + 站列表(按距离排序) + 点击查看桩详情
class NearbyStationsPage : public QWidget
{
    Q_OBJECT

public:
    explicit NearbyStationsPage(QWidget *parent = nullptr);

    // 供导航页复用最新站点数据(含坐标与距离)
    QList<StationInfo> stations() const { return m_stations; }
    double currentLon() const { return m_lon; }
    double currentLat() const { return m_lat; }

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refresh();
    void onStationSelected();
    void showPileDetail();

private:
    QComboBox *m_regionCombo;
    QTableWidget *m_table;

    QList<StationInfo> m_stations;
    double m_lon = 123.4500;
    double m_lat = 41.7000;
};

#endif // NEARBYSTATIONSPAGE_H
