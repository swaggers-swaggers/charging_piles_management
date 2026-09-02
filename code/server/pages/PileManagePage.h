#ifndef PILEMANAGEPAGE_H
#define PILEMANAGEPAGE_H

#include <QWidget>

class QTableWidget;
class QPushButton;
class QString;

// 充电桩管理页: 电桩列表(联表电站名) + 选中电桩执行"远程重启"(模拟指令)
class PileManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit PileManagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onSelectionChanged();
    void onRestartClicked();

private:
    QTableWidget *m_table;
    QPushButton *m_restartBtn;
    int m_selectedId = -1;
    QString m_selectedCode;
    int m_selectedStatus = -1;
};

#endif // PILEMANAGEPAGE_H
