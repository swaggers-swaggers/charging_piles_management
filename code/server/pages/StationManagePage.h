#ifndef STATIONMANAGEPAGE_H
#define STATIONMANAGEPAGE_H

#include <QWidget>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
struct PileInfo;

// 充电站与电桩统一管理: 站点运行概况 + 电桩搜索/状态筛选 + 故障/恢复/重启。
class StationManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit StationManagePage(QWidget *parent = nullptr);

public slots:
    void refreshPage();

private slots:
    void refresh();
    void onAddStation();
    void onStationSelected();
    void onPileSelected();
    void onRestartPile();
    void onTogglePileFault();

private:
    void loadPileDetail(int stationId, const QString &stationName);
    bool pileMatchesFilter(const PileInfo &pile, bool stationMatches) const;

    QLineEdit *m_searchEdit;
    QComboBox *m_statusFilter;
    QTableWidget *m_stationTable;
    QTableWidget *m_pileTable;
    QLabel *m_detailTitle;
    QPushButton *m_restartBtn;
    QPushButton *m_faultBtn;
    int m_selectedStationId = -1;
    int m_selectedPileId = -1;
    int m_selectedPileStatus = -1;
    QString m_selectedPileCode;
};

#endif // STATIONMANAGEPAGE_H
