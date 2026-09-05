#include "ChargingPage.h"

#include "ClientSession.h"
#include "IconFactory.h"
#include "protocol.h"
#include "network/TcpClient.h"

#include <QButtonGroup>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtMath>

namespace {
PileInfo findPile(const QList<PileInfo> &piles, int id)
{
    for (const PileInfo &p : piles)
        if (p.id == id)
            return p;
    return PileInfo();
}

QString targetDesc(const OrderInfo &o)
{
    switch (o.targetType) {
    case TargetEnergy:  return QStringLiteral("目标 %1 度").arg(o.targetValue, 0, 'f', 1);
    case TargetAmount:  return QStringLiteral("目标 %1 元").arg(o.targetValue, 0, 'f', 1);
    case TargetMinutes: return QStringLiteral("目标 %1 分钟").arg(int(o.targetValue));
    default:            return QStringLiteral("手动结束");
    }
}
} // namespace

// ============================================================================
// ChargeRingWidget 环形进度
// ============================================================================
ChargeRingWidget::ChargeRingWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(220, 220);
}

void ChargeRingWidget::setProgress(double progress)
{
    m_progress = progress;
    update();
}

void ChargeRingWidget::setCenterText(const QString &big, const QString &small)
{
    m_big = big;
    m_small = small;
    update();
}

void ChargeRingWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(width() / 2.0, height() / 2.0);
    const qreal r = qMin(width(), height()) / 2.0 - 16;
    const QRectF ringRect(c.x() - r, c.y() - r, r * 2, r * 2);

    QPen bg(QColor("#E6EDF5"), 14, Qt::SolidLine, Qt::RoundCap);
    p.setPen(bg);
    p.drawArc(ringRect, 0, 360 * 16);

    if (m_progress >= 0) {
        QPen fg(QColor("#2E7BE6"), 14, Qt::SolidLine, Qt::RoundCap);
        p.setPen(fg);
        const int span = int(qBound(0.0, m_progress, 1.0) * 360 * 16);
        p.drawArc(ringRect, 90 * 16, -span);
    } else {
        QPen fg(QColor("#37C6FF"), 14, Qt::SolidLine, Qt::RoundCap);
        p.setPen(fg);
        p.drawArc(ringRect, 90 * 16, -60 * 16);
    }

    QFont big = p.font();
    big.setPointSize(22);
    big.setBold(true);
    p.setPen(QColor("#1F2A3C"));
    p.setFont(big);
    p.drawText(QRectF(c.x() - r, c.y() - 22, r * 2, 34), Qt::AlignCenter, m_big);

    QFont small = p.font();
    small.setPointSize(10);
    small.setBold(false);
    p.setPen(QColor("#6B7280"));
    p.setFont(small);
    p.drawText(QRectF(c.x() - r, c.y() + 14, r * 2, 24), Qt::AlignCenter, m_small);
}

// ============================================================================
// 充电设置对话框
// ============================================================================
class ChargeSetupDialog : public QDialog
{
public:
    ChargeSetupDialog(const PileInfo &pile, double basePrice, double balance, QWidget *parent)
        : QDialog(parent), m_pile(pile), m_price(basePrice), m_balance(balance)
    {
        setWindowTitle(QStringLiteral("充电设置 - %1").arg(pile.code));
        setMinimumWidth(340);

        QFormLayout *form = new QFormLayout(this);
        m_type = new QComboBox(this);
        m_type->addItem(QStringLiteral("不限(手动结束)"), TargetNone);
        m_type->addItem(QStringLiteral("按电量(度)"), TargetEnergy);
        m_type->addItem(QStringLiteral("按金额(元)"), TargetAmount);
        m_type->addItem(QStringLiteral("按时长(分钟)"), TargetMinutes);

        m_value = new QDoubleSpinBox(this);
        m_value->setDecimals(1);

        QLabel *pileLabel = new QLabel(
            QStringLiteral("%1 · %2 · 功率 %3 kW")
                .arg(pile.code, pile.type == PileFast ? QStringLiteral("快充") : QStringLiteral("慢充"))
                .arg(pile.power, 0, 'f', 1), this);
        QLabel *priceLabel = new QLabel(
            QStringLiteral("基准电价 %1 元/度(实际按站点分时费率结算)").arg(basePrice, 0, 'f', 2), this);
        m_estimate = new QLabel(this);

        form->addRow(pileLabel);
        form->addRow(QStringLiteral("充电目标:"), m_type);
        form->addRow(QStringLiteral("目标数值:"), m_value);
        form->addRow(priceLabel);
        form->addRow(QStringLiteral("预授权冻结:"), m_estimate);

        QDialogButtonBox *box = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始充电"));
        box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        form->addRow(box);
        connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { syncRange(); });
        syncRange();
    }

    int targetType() const { return m_type->currentData().toInt(); }
    double targetValue() const { return targetType() == TargetNone ? 0.0 : m_value->value(); }

private:
    void syncRange()
    {
        const int t = m_type->currentData().toInt();
        m_value->setEnabled(t != TargetNone);
        if (t == TargetEnergy) {
            m_value->setRange(1, 500); m_value->setSuffix(" 度"); m_value->setValue(30);
        } else if (t == TargetAmount) {
            m_value->setRange(1, 10000); m_value->setSuffix(" 元"); m_value->setValue(30);
        } else if (t == TargetMinutes) {
            m_value->setRange(1, 1440); m_value->setSuffix(" 分钟"); m_value->setValue(60);
        }
        double freeze = 0;
        if (t == TargetNone) freeze = qMin(Protocol::ChargeConfig::kDefaultFreeze, m_balance);
        else if (t == TargetEnergy) freeze = m_value->value() * m_price * 1.2;
        else if (t == TargetAmount) freeze = m_value->value();
        else freeze = m_pile.power * m_value->value() / 60.0 * m_price * 1.2;
        const bool enough = freeze <= m_balance + 1e-6;
        m_estimate->setText(QStringLiteral("%1 元    (账户余额 %2 元)")
                                .arg(freeze, 0, 'f', 2).arg(m_balance, 0, 'f', 2));
        m_estimate->setStyleSheet(enough ? "color:#1F9D67;" : "color:#C5525A;font-weight:bold;");
    }

    PileInfo m_pile;
    double m_price;
    double m_balance;
    QComboBox *m_type;
    QDoubleSpinBox *m_value;
    QLabel *m_estimate;
};

// ============================================================================
// 时段预约对话框
// ============================================================================
class AppointDialog : public QDialog
{
public:
    AppointDialog(int pileId, const QString &pileCode, QWidget *parent)
        : QDialog(parent), m_pileId(pileId)
    {
        setWindowTitle(QStringLiteral("预约充电 - %1").arg(pileCode));
        setMinimumWidth(470);
        QVBoxLayout *lay = new QVBoxLayout(this);

        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new QLabel(QStringLiteral("日期:"), this));
        m_date = new QDateEdit(QDate::currentDate(), this);
        m_date->setDisplayFormat("yyyy-MM-dd");
        m_date->setCalendarPopup(true);
        m_date->setMinimumDate(QDate::currentDate());
        m_date->setMaximumDate(QDate::currentDate().addDays(3));
        row->addWidget(m_date);
        row->addSpacing(16);
        row->addWidget(new QLabel(QStringLiteral("时长:"), this));
        m_dur = new QComboBox(this);
        m_dur->addItem("30 分钟", 30);
        m_dur->addItem("60 分钟", 60);
        m_dur->addItem("90 分钟", 90);
        m_dur->addItem("120 分钟", 120);
        m_dur->setCurrentIndex(1);
        row->addWidget(m_dur);
        row->addStretch();
        lay->addLayout(row);

        m_hint = new QLabel(QStringLiteral("绿色为可约时段, 灰色已被占用, 点击选择开始时间"), this);
        m_hint->setStyleSheet("color:#6B7280;");
        lay->addWidget(m_hint);

        QWidget *gridHost = new QWidget(this);
        m_grid = new QGridLayout(gridHost);
        m_grid->setSpacing(6);
        QScrollArea *scroll = new QScrollArea(this);
        scroll->setWidget(gridHost);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setMinimumHeight(230);
        lay->addWidget(scroll);

        QDialogButtonBox *box = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认预约"));
        box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        lay->addWidget(box);
        connect(box, &QDialogButtonBox::accepted, this, [this] {
            if (m_selectedIndex < 0) {
                QMessageBox::warning(this, QStringLiteral("提示"),
                                     QStringLiteral("请先点选一个开始时段"));
                return;
            }
            const int spanCount = m_dur->currentData().toInt() / 30;
            if (m_selectedIndex + spanCount > m_slotLabels.size()) {
                QMessageBox::warning(this, QStringLiteral("提示"),
                                     QStringLiteral("营业到 22:00, 该时长超出范围"));
                return;
            }
            for (int i = m_selectedIndex; i < m_selectedIndex + spanCount; ++i) {
                if (m_blocked.contains(i)) {
                    QMessageBox::warning(this, QStringLiteral("提示"),
                                         QStringLiteral("所选区间内有已被占用时段, 请更换"));
                    return;
                }
            }
            start = m_slotLabels[m_selectedIndex];
            const int endIdx = m_selectedIndex + spanCount;
            end = endIdx < m_slotLabels.size() ? m_slotLabels[endIdx] : QStringLiteral("22:00");
            date = m_date->date().toString("yyyy-MM-dd");
            accept();
        });
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_date, &QDateEdit::dateChanged, this, [this](const QDate &) { reloadSlots(); });
        connect(m_dur, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { paintSelection(); });
        reloadSlots();
    }

    QString date, start, end;

private:
    void reloadSlots()
    {
        m_selectedIndex = -1;
        m_blocked.clear();
        m_slotLabels.clear();
        QLayoutItem *it = nullptr;
        while ((it = m_grid->takeAt(0)) != nullptr) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        m_slotBtns.clear();

        QJsonObject req;
        req.insert("pileId", m_pileId);
        req.insert("date", m_date->date().toString("yyyy-MM-dd"));
        const QJsonObject reply = TcpClient::instance().request(Protocol::ReqAppointSlots, req);
        if (!reply.value("ok").toBool()) {
            m_hint->setText(QStringLiteral("时段加载失败: ") + reply.value("error").toString());
            return;
        }
        const QJsonArray slotArr = reply.value("slots").toArray();
        for (const QJsonValue &v : slotArr)
            m_slotLabels.append(v.toString());
        const QJsonArray booked = reply.value("booked").toArray();
        for (const QJsonValue &v : booked) {
            const QJsonObject b = v.toObject();
            const int bs = indexOf(b.value("start").toString());
            const int be = indexOf(b.value("end").toString());
            for (int i = bs; i < be && i >= 0; ++i)
                m_blocked.insert(i);
        }

        for (int i = 0; i < m_slotLabels.size(); ++i) {
            QPushButton *btn = new QPushButton(m_slotLabels[i], this);
            btn->setCheckable(true);
            btn->setFixedHeight(34);
            if (m_blocked.contains(i)) {
                btn->setEnabled(false);
                btn->setText(m_slotLabels[i] + QStringLiteral(" 已约"));
                btn->setStyleSheet(
                    "QPushButton{background:#EEF0F3;color:#9AA3AF;border:1px solid #E2E6EC;"
                    "border-radius:6px;} QPushButton:disabled{color:#9AA3AF;}");
            } else {
                btn->setStyleSheet(
                    "QPushButton{background:#EAF7F0;color:#1F9D67;border:1px solid #BFE6D2;"
                    "border-radius:6px;} QPushButton:checked{background:#2E7BE6;color:white;"
                    "border:1px solid #2E7BE6;font-weight:bold;}");
                connect(btn, &QPushButton::clicked, this, [this, i] {
                    m_selectedIndex = i;
                    paintSelection();
                });
            }
            m_slotBtns.append(btn);
            m_grid->addWidget(btn, i / 4, i % 4);
        }
    }

    void paintSelection()
    {
        if (m_selectedIndex < 0)
            return;
        const int spanCount = m_dur->currentData().toInt() / 30;
        for (int i = 0; i < m_slotBtns.size(); ++i) {
            QPushButton *b = m_slotBtns[i];
            if (!b->isEnabled())
                continue;
            if (i == m_selectedIndex)
                b->setStyleSheet("QPushButton{background:#2E7BE6;color:white;border:1px solid #2E7BE6;"
                                 "border-radius:6px;font-weight:bold;}");
            else if (i > m_selectedIndex && i < m_selectedIndex + spanCount)
                b->setStyleSheet("QPushButton{background:#BCD7FA;color:#1B5BB8;border:1px solid #8FBCF2;"
                                 "border-radius:6px;font-weight:bold;}");
            else
                b->setStyleSheet("QPushButton{background:#EAF7F0;color:#1F9D67;border:1px solid #BFE6D2;"
                                 "border-radius:6px;}");
            b->setChecked(i == m_selectedIndex);
        }
    }

    int indexOf(const QString &hhmm) const
    {
        const int idx = m_slotLabels.indexOf(hhmm);
        if (idx >= 0)
            return idx;
        if (hhmm == "22:00")
            return m_slotLabels.size();
        return -1;
    }

    int m_pileId;
    QDateEdit *m_date = nullptr;
    QComboBox *m_dur = nullptr;
    QLabel *m_hint = nullptr;
    QGridLayout *m_grid = nullptr;
    QStringList m_slotLabels;
    QList<QPushButton *> m_slotBtns;
    QSet<int> m_blocked;
    int m_selectedIndex = -1;
};

// ============================================================================
// ChargingPage
// ============================================================================
ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    this->setStyleSheet(
        "QFrame#pileCard{background:white;border:1px solid #E4E7ED;border-radius:12px;}"
        "QFrame#miniCard{background:white;border:1px solid #E4E7ED;border-radius:12px;}"
        "QLabel#cardCode{font-size:17px;font-weight:bold;color:#1F2A3C;}"
        "QLabel#badgeIdle{background:#E7F7EF;color:#1F9D67;border-radius:8px;padding:2px 8px;}"
        "QLabel#badgeBusy{background:#FBF1DF;color:#B0863F;border-radius:8px;padding:2px 8px;}"
        "QLabel#badgeFault{background:#FBEAEB;color:#C5525A;border-radius:8px;padding:2px 8px;}"
        "QLabel#miniValue{font-size:20px;font-weight:bold;color:#1F2A3C;}"
        "QLabel#miniCap{color:#6B7280;font-size:12px;}"
        "QPushButton#primaryBtn{background:#2E7BE6;color:white;border:none;border-radius:8px;"
        "padding:7px 14px;font-weight:bold;} QPushButton#primaryBtn:hover{background:#1B5BB8;}"
        "QPushButton#ghostBtn{background:white;color:#2E7BE6;border:1px solid #2E7BE6;border-radius:8px;"
        "padding:6px 12px;} QPushButton#ghostBtn:hover{background:#EEF5FF;}"
        "QPushButton#warnBtn{background:#EFA93C;color:white;border:none;border-radius:8px;padding:7px 14px;}"
        "QPushButton#settleBtn{background:#C5525A;color:white;border:none;border-radius:10px;"
        "padding:12px;font-size:15px;font-weight:bold;} QPushButton#settleBtn:hover{background:#A93E46;}");

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack);

    buildSelectView();
    buildChargingView();
    buildWaitingView();
    m_stack->addWidget(m_selectView);
    m_stack->addWidget(m_chargingView);
    m_stack->addWidget(m_waitingView);

    connect(&TcpClient::instance(), &TcpClient::pushReceived,
            this, &ChargingPage::onPushReceived);
}

void ChargingPage::buildSelectView()
{
    m_selectView = new QWidget(this);
    QVBoxLayout *lay = new QVBoxLayout(m_selectView);
    lay->setContentsMargins(24, 20, 24, 24);
    lay->setSpacing(14);

    QLabel *title = new QLabel(QStringLiteral("电动汽车充电"), m_selectView);
    title->setObjectName("pageTitle");
    QLabel *hint = new QLabel(
        QStringLiteral("选择充电站与电桩: 空闲桩可立即充电或预约, 在用桩可排队等待"), m_selectView);
    hint->setObjectName("pageHint");

    QHBoxLayout *stationRow = new QHBoxLayout();
    stationRow->addWidget(new QLabel(QStringLiteral("充电站:"), m_selectView));
    m_stationCombo = new QComboBox(m_selectView);
    m_stationCombo->setObjectName("stationCombo");
    m_stationCombo->setMinimumWidth(300);
    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), m_selectView);
    refreshBtn->setObjectName("searchButton");
    stationRow->addWidget(m_stationCombo);
    stationRow->addWidget(refreshBtn);
    stationRow->addStretch();
    m_stationInfo = new QLabel(m_selectView);
    m_stationInfo->setStyleSheet("color:#6B7280;");
    stationRow->addWidget(m_stationInfo);

    m_cardScroll = new QScrollArea(m_selectView);
    m_cardScroll->setWidgetResizable(true);
    m_cardScroll->setFrameShape(QFrame::NoFrame);
    m_cardHost = new QWidget(m_cardScroll);
    m_cardGrid = new QGridLayout(m_cardHost);
    m_cardGrid->setContentsMargins(2, 2, 2, 2);
    m_cardGrid->setSpacing(12);
    m_cardGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_cardScroll->setWidget(m_cardHost);

    lay->addWidget(title);
    lay->addWidget(hint);
    lay->addLayout(stationRow);
    lay->addWidget(m_cardScroll, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &ChargingPage::refreshStations);
    connect(m_stationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onStationPicked(); });
}

void ChargingPage::buildChargingView()
{
    m_chargingView = new QWidget(this);
    QVBoxLayout *lay = new QVBoxLayout(m_chargingView);
    lay->setContentsMargins(24, 20, 24, 24);
    lay->setSpacing(14);

    QLabel *title = new QLabel(QStringLiteral("充电中"), m_chargingView);
    title->setObjectName("pageTitle");
    m_orderTitle = new QLabel(m_chargingView);
    m_orderTitle->setAlignment(Qt::AlignCenter);
    m_orderTitle->setStyleSheet("font-size:14px;color:#475569;");

    m_ring = new ChargeRingWidget(m_chargingView);

    auto makeMini = [this](const QString &cap, QLabel **valOut) {
        QFrame *card = new QFrame(m_chargingView);
        card->setObjectName("miniCard");
        card->setFixedHeight(82);
        QVBoxLayout *v = new QVBoxLayout(card);
        v->setContentsMargins(16, 12, 16, 12);
        QLabel *capL = new QLabel(cap, card);
        capL->setObjectName("miniCap");
        QLabel *val = new QLabel("-", card);
        val->setObjectName("miniValue");
        val->setAlignment(Qt::AlignCenter);
        v->addWidget(capL);
        v->addWidget(val);
        *valOut = val;
        return card;
    };
    QHBoxLayout *cards = new QHBoxLayout();
    cards->setSpacing(12);
    cards->addWidget(makeMini(QStringLiteral("已充电量(度)"), &m_energyVal), 1);
    cards->addWidget(makeMini(QStringLiteral("当前费用(元)"), &m_amountVal), 1);
    cards->addWidget(makeMini(QStringLiteral("充电时长(分)"), &m_minutesVal), 1);

    m_priceHint = new QLabel(m_chargingView);
    m_priceHint->setAlignment(Qt::AlignCenter);
    m_priceHint->setStyleSheet("color:#6B7280;");

    QPushButton *stopBtn = new QPushButton(QStringLiteral("结束充电并结算"), m_chargingView);
    stopBtn->setObjectName("settleBtn");
    stopBtn->setCursor(Qt::PointingHandCursor);

    lay->addWidget(title);
    lay->addWidget(m_orderTitle);
    lay->addWidget(m_ring, 0, Qt::AlignHCenter);
    lay->addLayout(cards);
    lay->addWidget(m_priceHint);
    lay->addStretch();
    lay->addWidget(stopBtn);

    connect(stopBtn, &QPushButton::clicked, this, &ChargingPage::onStopCharge);
}

void ChargingPage::buildWaitingView()
{
    m_waitingView = new QWidget(this);
    QVBoxLayout *lay = new QVBoxLayout(m_waitingView);
    lay->setContentsMargins(24, 20, 24, 24);
    lay->setSpacing(16);

    QLabel *title = new QLabel(QStringLiteral("排队 / 预约"), m_waitingView);
    title->setObjectName("pageTitle");
    lay->addWidget(title);
    lay->addStretch();

    QLabel *icon = new QLabel(m_waitingView);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(IconFactory::icon(IconFactory::IconBattery, QColor("#2E7BE6")).pixmap(56, 56));
    m_waitTitle = new QLabel(m_waitingView);
    m_waitTitle->setAlignment(Qt::AlignCenter);
    QFont tf = m_waitTitle->font();
    tf.setPointSize(16);
    tf.setBold(true);
    m_waitTitle->setFont(tf);
    m_waitDesc = new QLabel(m_waitingView);
    m_waitDesc->setAlignment(Qt::AlignCenter);
    m_waitDesc->setStyleSheet("color:#6B7280;font-size:13px;");
    m_waitDesc->setWordWrap(true);

    m_cancelWaitBtn = new QPushButton(m_waitingView);
    m_cancelWaitBtn->setObjectName("warnBtn");
    m_cancelWaitBtn->setCursor(Qt::PointingHandCursor);
    m_cancelWaitBtn->setFixedWidth(220);

    lay->addWidget(icon, 0, Qt::AlignHCenter);
    lay->addWidget(m_waitTitle);
    lay->addWidget(m_waitDesc);
    lay->addSpacing(10);
    lay->addWidget(m_cancelWaitBtn, 0, Qt::AlignHCenter);
    lay->addStretch();

    connect(m_cancelWaitBtn, &QPushButton::clicked, this, &ChargingPage::onCancelWaiting);
}

void ChargingPage::refreshStations()
{
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationList, QJsonObject{{"lon", 123.45}, {"lat", 41.70}});
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("查询失败"), reply.value("error").toString());
        return;
    }
    m_stations.clear();
    const QJsonArray arr = reply.value("stations").toArray();
    for (const QJsonValue &v : arr)
        m_stations.append(StationInfo::fromJson(v.toObject()));

    m_stationCombo->blockSignals(true);
    m_stationCombo->clear();
    for (const StationInfo &s : m_stations)
        m_stationCombo->addItem(QString("%1 (空闲 %2/%3)")
                                    .arg(s.name).arg(s.idlePiles).arg(s.totalPiles), s.id);
    m_stationCombo->blockSignals(false);
    onStationPicked();
}

void ChargingPage::onStationPicked()
{
    m_piles.clear();
    const int idx = m_stationCombo->currentIndex();
    if (idx < 0 || idx >= m_stations.size()) {
        rebuildPileCards();
        return;
    }
    const StationInfo &s = m_stations[idx];
    m_stationInfo->setText(QStringLiteral("基准电价 %1 元/度").arg(s.price, 0, 'f', 2));

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationPiles, QJsonObject{{"stationId", s.id}});
    if (reply.value("ok").toBool()) {
        const QJsonArray piles = reply.value("piles").toArray();
        for (const QJsonValue &v : piles)
            m_piles.append(PileInfo::fromJson(v.toObject()));
    }
    rebuildPileCards();
}

void ChargingPage::rebuildPileCards()
{
    QLayoutItem *it = nullptr;
    while ((it = m_cardGrid->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    const int columns = 3;
    for (int i = 0; i < m_piles.size(); ++i) {
        const PileInfo p = m_piles[i];
        QFrame *card = new QFrame(m_cardHost);
        card->setObjectName("pileCard");
        card->setMinimumWidth(210);
        card->setMaximumWidth(330);
        const QString border = p.status == PileIdle ? "#BFE6D2"
                               : p.status == PileInUse ? "#F0DDB8" : "#EBC9CB";
        card->setStyleSheet(QString("QFrame#pileCard{background:white;border:1.5px solid %1;"
                                    "border-radius:12px;}").arg(border));

        QVBoxLayout *cv = new QVBoxLayout(card);
        cv->setContentsMargins(14, 12, 14, 12);
        cv->setSpacing(8);

        QHBoxLayout *top = new QHBoxLayout();
        QLabel *code = new QLabel(p.code, card);
        code->setObjectName("cardCode");
        QLabel *badge = new QLabel(card);
        if (p.status == PileIdle) {
            badge->setText(QStringLiteral("空闲")); badge->setObjectName("badgeIdle");
        } else if (p.status == PileInUse) {
            badge->setText(QStringLiteral("充电中")); badge->setObjectName("badgeBusy");
        } else {
            badge->setText(QStringLiteral("故障")); badge->setObjectName("badgeFault");
        }
        top->addWidget(code);
        top->addStretch();
        top->addWidget(badge);

        QLabel *spec = new QLabel(
            QStringLiteral("%1 · %2 kW · 累计使用 %3 次")
                .arg(p.type == PileFast ? QStringLiteral("快充") : QStringLiteral("慢充"))
                .arg(p.power, 0, 'f', 1).arg(p.totalCount), card);
        spec->setStyleSheet("color:#6B7280;font-size:12px;");

        QHBoxLayout *btns = new QHBoxLayout();
        btns->setSpacing(8);
        if (p.status == PileIdle) {
            QPushButton *start = new QPushButton(QStringLiteral("立即充电"), card);
            start->setObjectName("primaryBtn");
            start->setCursor(Qt::PointingHandCursor);
            QPushButton *appoint = new QPushButton(QStringLiteral("预约"), card);
            appoint->setObjectName("ghostBtn");
            appoint->setCursor(Qt::PointingHandCursor);
            connect(start, &QPushButton::clicked, this, [this, id = p.id] { openChargeSetup(id); });
            connect(appoint, &QPushButton::clicked, this, [this, id = p.id] { openAppointDialog(id); });
            btns->addWidget(start);
            btns->addWidget(appoint);
        } else if (p.status == PileInUse) {
            QPushButton *queue = new QPushButton(QStringLiteral("排队等待"), card);
            queue->setObjectName("warnBtn");
            queue->setCursor(Qt::PointingHandCursor);
            QPushButton *appoint = new QPushButton(QStringLiteral("预约时段"), card);
            appoint->setObjectName("ghostBtn");
            appoint->setCursor(Qt::PointingHandCursor);
            connect(queue, &QPushButton::clicked, this, [this, id = p.id] { joinQueue(id); });
            connect(appoint, &QPushButton::clicked, this, [this, id = p.id] { openAppointDialog(id); });
            btns->addWidget(queue);
            btns->addWidget(appoint);
        } else {
            QLabel *fault = new QLabel(QStringLiteral("设备检修中, 暂不可用"), card);
            fault->setStyleSheet("color:#C5525A;font-size:12px;");
            btns->addWidget(fault);
            btns->addStretch();
        }

        cv->addLayout(top);
        cv->addWidget(spec);
        cv->addLayout(btns);
        m_cardGrid->addWidget(card, i / columns, i % columns);
    }
}

void ChargingPage::openChargeSetup(int pileId)
{
    const int idx = m_stationCombo->currentIndex();
    const double basePrice = (idx >= 0) ? m_stations[idx].price : 1.2;
    ChargeSetupDialog dlg(findPile(m_piles, pileId), basePrice,
                          ClientSession::instance().balance, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    doStart(pileId, dlg.targetType(), dlg.targetValue());
}

void ChargingPage::doStart(int pileId, int targetType, double targetValue)
{
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("pileId", pileId);
    req.insert("targetType", targetType);
    req.insert("targetValue", targetValue);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqStartChargeExt, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("无法开始充电"), reply.value("error").toString());
        onStationPicked();   // 桩可能刚被抢占, 刷新状态
        return;
    }
    ClientSession::instance().balance = reply.value("balance").toDouble(
        ClientSession::instance().balance);
    m_currentOrder = OrderInfo::fromJson(reply.value("order").toObject());
    m_hasOrder = true;
    m_waitingId = -1;
    enterChargingView(m_currentOrder);
}

void ChargingPage::joinQueue(int pileId)
{
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("pileId", pileId);
    req.insert("action", 0);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqReservePile, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("排队失败"), reply.value("error").toString());
        return;
    }
    QMessageBox::information(this, QStringLiteral("排队成功"),
                             QStringLiteral("已加入排队, 当前第 %1 位, 轮到您时将自动提醒")
                                 .arg(reply.value("queuePos").toInt()));
    refreshWaiting();
}

void ChargingPage::openAppointDialog(int pileId)
{
    const PileInfo pile = findPile(m_piles, pileId);
    AppointDialog dlg(pileId, pile.code, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("pileId", pileId);
    req.insert("reserveDate", dlg.date);
    req.insert("reserveStart", dlg.start);
    req.insert("reserveEnd", dlg.end);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqAppointPile, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("预约失败"), reply.value("error").toString());
        return;
    }
    QMessageBox::information(this, QStringLiteral("预约成功"),
                             QStringLiteral("已预约 %1 %2~%3, 开始前 10 分钟将提醒您")
                                 .arg(dlg.date, dlg.start, dlg.end));
    refreshWaiting();
}

void ChargingPage::onStopCharge()
{
    if (!m_hasOrder)
        return;
    if (QMessageBox::question(this, QStringLiteral("结算确认"),
                              QStringLiteral("确定结束充电并结算订单 #%1 吗?").arg(m_currentOrder.id))
        != QMessageBox::Yes)
        return;
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("orderId", m_currentOrder.id);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqStopCharge, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("结算失败"), reply.value("error").toString());
        return;
    }
    const OrderInfo order = OrderInfo::fromJson(reply.value("order").toObject());
    showSettlement(order, reply.value("balance").toDouble());
}

void ChargingPage::onCancelWaiting()
{
    if (m_waitingId < 0)
        return;
    const QString tip = m_waitingType == ReserveAppoint
                            ? QStringLiteral("确定取消该预约吗?") : QStringLiteral("确定退出排队吗?");
    if (QMessageBox::question(this, QStringLiteral("提示"), tip) != QMessageBox::Yes)
        return;
    QJsonObject req;
    req.insert("userId", ClientSession::instance().userId);
    req.insert("action", 1);
    req.insert("reservationId", m_waitingId);
    const QJsonObject reply = TcpClient::instance().request(Protocol::ReqReservePile, req);
    if (!reply.value("ok").toBool()) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), reply.value("error").toString());
        return;
    }
    m_waitingId = -1;
    enterSelectView();
    refreshStations();
}

void ChargingPage::enterChargingView(const OrderInfo &order)
{
    m_currentOrder = order;
    m_hasOrder = true;
    m_orderTitle->setText(QStringLiteral("订单 #%1    %2    电桩 %3")
                              .arg(order.id).arg(order.stationName, order.pileCode));
    m_energyVal->setText(QString::number(order.energy, 'f', 2));
    m_amountVal->setText(QString::number(order.amount, 'f', 2));
    m_minutesVal->setText(QString::number(order.simMinutes));
    m_ring->setCenterText(QString::number(order.energy, 'f', 1), QStringLiteral("度"));
    m_ring->setProgress(order.targetType == TargetNone ? -1 : 0);
    m_priceHint->setText(QStringLiteral("单价 %1 元/度 · 冻结 %2 元 · %3")
                             .arg(order.priceSnapshot, 0, 'f', 2)
                             .arg(order.freezeAmount, 0, 'f', 2)
                             .arg(targetDesc(order)));
    m_stack->setCurrentIndex(1);
}

void ChargingPage::enterSelectView()
{
    m_hasOrder = false;
    m_stack->setCurrentIndex(0);
}

void ChargingPage::enterWaitingView(const ReservationInfo &r)
{
    m_waitingId = r.id;
    m_waitingPileId = r.pileId;
    m_waitingType = r.type;
    if (r.type == ReserveAppoint) {
        m_waitTitle->setText(QStringLiteral("时段预约成功"));
        m_waitDesc->setText(QStringLiteral(
            "电桩 %1\n预约时段: %2 %3 ~ %4\n开始前 10 分钟将提醒您, 请按时到场扫码充电")
            .arg(r.pileCode, r.reserveDate, r.reserveStart, r.reserveEnd));
        m_cancelWaitBtn->setText(QStringLiteral("取消预约"));
    } else {
        m_waitTitle->setText(QStringLiteral("现场排队中"));
        m_waitDesc->setText(QStringLiteral(
            "电桩 %1\n当前排队第 %2 位\n电桩释放轮到您时, 将在 30 秒内提醒确认")
            .arg(r.pileCode).arg(qMax(1, r.queuePos)));
        m_cancelWaitBtn->setText(QStringLiteral("退出排队"));
    }
    m_stack->setCurrentIndex(2);
}

void ChargingPage::refreshWaiting()
{
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqMyReservations,
        QJsonObject{{"userId", ClientSession::instance().userId}});
    if (!reply.value("ok").toBool())
        return;
    const QJsonArray arr = reply.value("reservations").toArray();
    for (const QJsonValue &v : arr) {
        const ReservationInfo r = ReservationInfo::fromJson(v.toObject());
        if (r.status == ReservationActive || r.status == ReservationAssigned) {
            enterWaitingView(r);
            return;
        }
    }
}

void ChargingPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUnfinishedOrder,
        QJsonObject{{"userId", ClientSession::instance().userId}});
    if (reply.value("ok").toBool() && reply.value("hasOrder").toBool()) {
        m_currentOrder = OrderInfo::fromJson(reply.value("order").toObject());
        enterChargingView(m_currentOrder);
        return;
    }

    // 无在充订单: 检查是否有有效排队/预约
    const QJsonObject res = TcpClient::instance().request(
        Protocol::ReqMyReservations,
        QJsonObject{{"userId", ClientSession::instance().userId}});
    bool waiting = false;
    if (res.value("ok").toBool()) {
        const QJsonArray arr = res.value("reservations").toArray();
        for (const QJsonValue &v : arr) {
            const ReservationInfo r = ReservationInfo::fromJson(v.toObject());
            if (r.status == ReservationActive || r.status == ReservationAssigned) {
                enterWaitingView(r);
                waiting = true;
                break;
            }
        }
    }
    if (!waiting) {
        enterSelectView();
        refreshStations();
    }
}

void ChargingPage::onPushReceived(const QJsonObject &msg)
{
    const int type = msg.value("type").toInt();

    if (type == Protocol::PushOrderProgress) {
        if (!m_hasOrder || msg.value("orderId").toInt() != m_currentOrder.id)
            return;
        m_currentOrder.energy = msg.value("energy").toDouble();
        m_currentOrder.amount = msg.value("amount").toDouble();
        m_currentOrder.simMinutes = msg.value("minutes").toInt();
        m_energyVal->setText(QString::number(m_currentOrder.energy, 'f', 2));
        m_amountVal->setText(QString::number(m_currentOrder.amount, 'f', 2));
        m_minutesVal->setText(QString::number(m_currentOrder.simMinutes));
        m_ring->setCenterText(QString::number(m_currentOrder.energy, 'f', 1),
                              QStringLiteral("度"));
        if (msg.contains("targetProgress")) {
            m_ring->setProgress(msg.value("targetProgress").toDouble());
        }
        return;
    }

    if (type != Protocol::PushOrderEvent)
        return;

    const int event = msg.value("event").toInt();
    if (event == 1) {
        // 排队轮到
        const int pileId = msg.value("pileId").toInt(m_waitingPileId);
        m_waitingId = -1;
        if (QMessageBox::question(this, QStringLiteral("轮到您了"),
                                  QStringLiteral("电桩已空闲! 是否立即开始充电?\n(超时未确认将顺延给下一位)"))
            == QMessageBox::Yes) {
            doStart(pileId, TargetNone, 0);
        } else {
            enterSelectView();
            refreshStations();
        }
    } else if (event == 2) {
        // 订单自动结束
        const OrderInfo order = OrderInfo::fromJson(msg.value("order").toObject());
        const QJsonObject info = TcpClient::instance().request(
            Protocol::ReqGetUserInfo, QJsonObject{});
        double balance = ClientSession::instance().balance;
        if (info.value("ok").toBool()) {
            balance = info.value("balance").toDouble(balance);
            ClientSession::instance().balance = balance;
        }
        showSettlement(order, balance);
    } else if (event == 3) {
        // 异常中断
        const OrderInfo order = OrderInfo::fromJson(msg.value("order").toObject());
        m_hasOrder = false;
        QMessageBox::warning(this, QStringLiteral("充电异常"),
                             QStringLiteral("订单 #%1 因故障中断, 已自动结算并释放冻结金额\n"
                                            "消费 %2 元, 如有疑问可联系管理员退款")
                                 .arg(order.id).arg(order.amount, 0, 'f', 2));
        enterSelectView();
        refreshStations();
    } else if (event == 4) {
        // 排队位置变化
        refreshWaiting();
    } else if (event == 6) {
        QMessageBox::information(this, QStringLiteral("预约提醒"),
                                 QStringLiteral("您预约的电桩 %1 将在 10 分钟后开放, 请准备到场")
                                     .arg(msg.value("pileCode").toString()));
    } else if (event == 7) {
        QMessageBox::information(this, QStringLiteral("排队/预约通知"),
                                 msg.value("message").toString(QStringLiteral("您的排队/预约已结束")));
        if (m_stack->currentIndex() == 2) {
            m_waitingId = -1;
            enterSelectView();
            refreshStations();
        }
    }
}

void ChargingPage::showSettlement(const OrderInfo &order, double balance)
{
    ClientSession::instance().balance = balance;
    m_hasOrder = false;

    QString reason;
    if (order.finishType == FinishByTarget) reason = QStringLiteral("已达到设定目标, 自动结束");
    else if (order.finishType == FinishByBalance) reason = QStringLiteral("冻结额度用尽, 自动结束");
    else if (order.finishType == FinishByAdmin) reason = QStringLiteral("管理员结束");
    else if (order.finishType == FinishByFault) reason = QStringLiteral("故障结束");
    else reason = QStringLiteral("用户手动结束");

    QMessageBox::information(
        this, QStringLiteral("结算成功"),
        QStringLiteral("订单 #%1 已完成\n\n电桩: %2\n结束方式: %3\n充电电量: %4 度\n"
                       "充电时长: %5 分钟\n消费金额: %6 元\n\n当前余额: %7 元")
            .arg(order.id).arg(order.pileCode, reason)
            .arg(order.energy, 0, 'f', 2).arg(order.simMinutes)
            .arg(order.amount, 0, 'f', 2).arg(balance, 0, 'f', 2));

    enterSelectView();
    refreshStations();
}
