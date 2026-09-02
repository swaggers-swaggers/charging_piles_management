#ifndef STATIONMANAGEPAGE_H
#define STATIONMANAGEPAGE_H

#include <QWidget>

class QLabel;
class QTableWidget;

// 充电站管理页: 站列表(在线率) + 点击行查看站内电桩明细 + 新增电站
class StationManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit StationManagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onAddStation();
    void onStationSelected();

private:
    void loadPileDetail(int stationId, const QString &stationName);

    QTableWidget *m_stationTable;
    QTableWidget *m_pileTable;
    QLabel *m_detailTitle;
    int m_selectedStationId = -1;
};

#endif // STATIONMANAGEPAGE_H
