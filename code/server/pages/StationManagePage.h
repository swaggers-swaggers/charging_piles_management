#ifndef STATIONMANAGEPAGE_H
#define STATIONMANAGEPAGE_H

#include <QWidget>

class QLabel;
class QTableWidget;
class QTimer;

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
    QTimer *m_autoRefresh = nullptr;   // 每 3 秒自动刷新, 无需手动点刷新按钮
};

#endif // STATIONMANAGEPAGE_H
