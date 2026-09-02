#ifndef NEARBYSTATIONSPAGE_H
#define NEARBYSTATIONSPAGE_H

#include "types.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;

// 附近充电站查询页(项目说明书):
//   定位: 下拉选择区域 或 手动输入地址(软件层面模拟 GPS + 腾讯地图 Web API 地址转坐标)
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
    void onLocate();            // 手动输入地址 → 模拟腾讯地图 Web API 解析坐标
    void onStationSelected();
    void showPileDetail();

private:
    QComboBox *m_regionCombo;
    QLineEdit *m_addrEdit;
    QTableWidget *m_table;

    QList<StationInfo> m_stations;
    double m_lon = 123.4500;
    double m_lat = 41.7000;
};

#endif // NEARBYSTATIONSPAGE_H
