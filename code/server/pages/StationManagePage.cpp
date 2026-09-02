#include "StationManagePage.h"

#include "PileDao.h"
#include "StationDao.h"

#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString pileStatusText(int status)
{
    switch (status) {
    case PileIdle:  return "闲置";
    case PileInUse: return "在用";
    case PileFault: return "故障";
    }
    return "未知";
}
} // namespace

StationManagePage::StationManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("充电站管理", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *refreshBtn = new QPushButton("刷新", this);
    QPushButton *addBtn = new QPushButton("新增电站", this);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(addBtn);
    btnRow->addStretch();

    m_stationTable = new QTableWidget(this);
    m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stationTable->verticalHeader()->setVisible(false);
    m_stationTable->setColumnCount(8);
    m_stationTable->setHorizontalHeaderLabels(
        { "ID", "站名", "详细地址", "经度", "纬度", "电价(元/度)", "总电桩数", "在线率" });

    // 右侧: 选中电站的电桩明细
    QWidget *detailPanel = new QWidget(this);
    QVBoxLayout *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(8);
    m_detailTitle = new QLabel("站内电桩明细(点击左侧电站行查看)", detailPanel);
    m_detailTitle->setObjectName("pageTitle");
    QFont detailFont = m_detailTitle->font();
    detailFont.setPointSize(detailFont.pointSize() - 4);
    m_detailTitle->setFont(detailFont);
    m_pileTable = new QTableWidget(detailPanel);
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->verticalHeader()->setVisible(false);
    m_pileTable->setColumnCount(6);
    m_pileTable->setHorizontalHeaderLabels(
        { "电桩编号", "类型", "功率(kW)", "状态", "累计次数", "累计时长(小时)" });
    detailLayout->addWidget(m_detailTitle);
    detailLayout->addWidget(m_pileTable, 1);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_stationTable);
    splitter->addWidget(detailPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(title);
    layout->addLayout(btnRow);
    layout->addWidget(splitter, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &StationManagePage::refresh);
    connect(addBtn, &QPushButton::clicked, this, &StationManagePage::onAddStation);
    connect(m_stationTable, &QTableWidget::itemSelectionChanged,
            this, &StationManagePage::onStationSelected);
    refresh();
}

void StationManagePage::refresh()
{
    // 故障数按站汇总, 用于计算在线率 = (总桩数 - 故障数) / 总桩数
    QHash<int, int> faultByStation;
    const QList<PileInfo> allPiles = PileDao::listAll();
    for (const PileInfo &p : allPiles) {
        if (p.status == PileFault)
            faultByStation[p.stationId]++;
    }

    const QList<StationInfo> stations = StationDao::list();
    m_stationTable->setRowCount(stations.size());
    for (int i = 0; i < stations.size(); ++i) {
        const StationInfo &s = stations[i];
        auto *idItem = new QTableWidgetItem(QString::number(s.id));
        idItem->setData(Qt::UserRole, s.id);
        m_stationTable->setItem(i, 0, idItem);
        m_stationTable->setItem(i, 1, new QTableWidgetItem(s.name));
        m_stationTable->setItem(i, 2, new QTableWidgetItem(s.address));
        m_stationTable->setItem(i, 3, new QTableWidgetItem(QString::number(s.longitude, 'f', 4)));
        m_stationTable->setItem(i, 4, new QTableWidgetItem(QString::number(s.latitude, 'f', 4)));
        m_stationTable->setItem(i, 5, new QTableWidgetItem(QString::number(s.price, 'f', 2)));
        m_stationTable->setItem(i, 6, new QTableWidgetItem(QString::number(s.totalPiles)));
        const int fault = faultByStation.value(s.id, 0);
        const double rate = s.totalPiles > 0
                                ? (s.totalPiles - fault) * 100.0 / s.totalPiles
                                : 100.0;
        m_stationTable->setItem(i, 7, new QTableWidgetItem(QString::number(rate, 'f', 1) + "%"));
    }
    m_stationTable->resizeColumnsToContents();
    onStationSelected();
}

void StationManagePage::onStationSelected()
{
    const QList<QTableWidgetItem *> selected = m_stationTable->selectedItems();
    if (selected.isEmpty()) {
        m_selectedStationId = -1;
        return;
    }
    const int row = selected.first()->row();
    m_selectedStationId = m_stationTable->item(row, 0)->data(Qt::UserRole).toInt();
    const QString name = m_stationTable->item(row, 1)->text();
    loadPileDetail(m_selectedStationId, name);
}

void StationManagePage::loadPileDetail(int stationId, const QString &stationName)
{
    m_detailTitle->setText(QString("站内电桩明细 - %1").arg(stationName));
    const QList<PileInfo> piles = PileDao::listByStation(stationId);
    m_pileTable->setRowCount(piles.size());
    for (int i = 0; i < piles.size(); ++i) {
        const PileInfo &p = piles[i];
        m_pileTable->setItem(i, 0, new QTableWidgetItem(p.code));
        m_pileTable->setItem(i, 1, new QTableWidgetItem(p.type == PileFast ? "快充" : "慢充"));
        m_pileTable->setItem(i, 2, new QTableWidgetItem(QString::number(p.power, 'f', 1)));
        m_pileTable->setItem(i, 3, new QTableWidgetItem(pileStatusText(p.status)));
        m_pileTable->setItem(i, 4, new QTableWidgetItem(QString::number(p.totalCount)));
        m_pileTable->setItem(i, 5,
                             new QTableWidgetItem(QString::number(p.totalDuration / 60.0, 'f', 1)));
    }
    m_pileTable->resizeColumnsToContents();
}

void StationManagePage::onAddStation()
{
    // 新增电站对话框
    QDialog dlg(this);
    dlg.setWindowTitle("新增电站");
    dlg.setFixedWidth(380);
    QFormLayout *form = new QFormLayout(&dlg);
    form->setSpacing(12);

    QLineEdit *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("请输入站名");
    QLineEdit *addrEdit = new QLineEdit(&dlg);
    addrEdit->setPlaceholderText("请输入详细地址");
    QDoubleSpinBox *lonSpin = new QDoubleSpinBox(&dlg);
    lonSpin->setRange(0.0, 180.0);
    lonSpin->setDecimals(4);
    lonSpin->setSingleStep(0.001);
    lonSpin->setValue(123.4500);
    QDoubleSpinBox *latSpin = new QDoubleSpinBox(&dlg);
    latSpin->setRange(0.0, 90.0);
    latSpin->setDecimals(4);
    latSpin->setSingleStep(0.001);
    latSpin->setValue(41.7000);
    QDoubleSpinBox *priceSpin = new QDoubleSpinBox(&dlg);
    priceSpin->setRange(0.1, 5.0);
    priceSpin->setDecimals(2);
    priceSpin->setSingleStep(0.05);
    priceSpin->setValue(1.00);
    priceSpin->setSuffix(" 元/度");
    QSpinBox *countSpin = new QSpinBox(&dlg);
    countSpin->setRange(1, 50);
    countSpin->setValue(6);

    form->addRow("站名", nameEdit);
    form->addRow("详细地址", addrEdit);
    form->addRow("经度", lonSpin);
    form->addRow("纬度", latSpin);
    form->addRow("电价", priceSpin);
    form->addRow("电桩数量", countSpin);

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton("确定", &dlg);
    QPushButton *cancelBtn = new QPushButton("取消", &dlg);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    form->addRow(btnRow);

    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (nameEdit->text().trimmed().isEmpty() || addrEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "站名和详细地址不能为空");
        return;
    }

    StationInfo s;
    s.name = nameEdit->text().trimmed();
    s.address = addrEdit->text().trimmed();
    s.longitude = lonSpin->value();
    s.latitude = latSpin->value();
    s.price = priceSpin->value();

    QString errMsg;
    if (!StationDao::add(&s, countSpin->value(), &errMsg)) {
        QMessageBox::warning(this, "新增失败", errMsg);
        return;
    }
    QMessageBox::information(this, "提示",
                             QString("充电站 %1 新增成功, 已生成 %2 个电桩")
                                 .arg(s.name).arg(countSpin->value()));
    refresh();
}
