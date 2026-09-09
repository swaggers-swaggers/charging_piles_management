#ifndef STATIONMANAGEPAGE_H
#define STATIONMANAGEPAGE_H

#include <QHash>
#include <QWidget>

class QLabel;
class QComboBox;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QScrollArea;
class QTableWidget;
struct PileInfo;

// 充电站与电桩统一管理: 多站点卡片 + 点击查看全宽电桩数据卡片。
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
    void onPileSelected();
    void onRestartPile();
    void onTogglePileFault();

private:
    void loadPileDetail(int stationId, const QString &stationName, bool stationMatches = false);
    void selectStationCard(int stationId);
    bool pileMatchesFilter(const PileInfo &pile, bool stationMatches) const;

    QLineEdit *m_searchEdit;
    QComboBox *m_statusFilter;
    QWidget *m_stationCardsHost;
    QHBoxLayout *m_stationCardsLayout;
    QScrollArea *m_stationCardsScroll;
    QTableWidget *m_pileTable;
    QLabel *m_detailTitle;
    QProgressBar *m_occupancyBar;
    QPushButton *m_restartBtn;
    QPushButton *m_faultBtn;
    int m_selectedStationId = -1;
    int m_selectedPileId = -1;
    int m_selectedPileStatus = -1;
    QString m_selectedPileCode;
    QHash<QString, QString> m_previousPileValues;
};

#endif // STATIONMANAGEPAGE_H
