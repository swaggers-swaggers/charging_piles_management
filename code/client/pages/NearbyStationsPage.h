#ifndef NEARBYSTATIONSPAGE_H
#define NEARBYSTATIONSPAGE_H

#include "types.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;

// 附近充电站查询页(项目说明书):
//   定位: 下拉选择区域 或输入内置演示地标，不依赖第三方地图 Key
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

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void refresh();
    void onRegionChanged(int index);
    void onLocate();            // 手动输入地址 → 本地演示地标匹配
    void onStationSelected();
    void showPileDetail();

private:
    QComboBox *m_regionCombo;
    QLineEdit *m_addrEdit;
    QTableWidget *m_table;

    QList<StationInfo> m_stations;
    double m_lon = 116.3100;
    double m_lat = 39.9600;
};

#endif // NEARBYSTATIONSPAGE_H
