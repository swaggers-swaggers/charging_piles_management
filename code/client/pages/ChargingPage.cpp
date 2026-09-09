#include "ChargingPage.h"
#include "ChargingParticles.h"

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
#include <QLineEdit>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QLinearGradient>
#include <QVariantAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
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

StationInfo findStation(const QList<StationInfo> &stations, int id)
{
    for (const StationInfo &station : stations)
        if (station.id == id)
            return station;
    return StationInfo();
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
    m_progressAnimation = new QVariantAnimation(this);
    // 服务端每 3 秒更新一次进度，稍长的补间可保证两次更新之间圆弧持续移动。
    m_progressAnimation->setDuration(3200);
    m_progressAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(m_progressAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_progress = value.toDouble();
                update();
            });
}

void ChargeRingWidget::setProgress(double progress)
{
    const double target = qBound(0.0, progress, 1.0);
    if (m_progressAnimation->state() == QAbstractAnimation::Running
        && qAbs(m_progressAnimation->endValue().toDouble() - target) < 0.0001)
        return;

    m_progressAnimation->stop();
    if (target + 0.001 < m_progress)
        m_progress = 0.0;
    m_progressAnimation->setStartValue(m_progress);
    m_progressAnimation->setEndValue(target);
    m_progressAnimation->start();
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

    if (m_progress > 0.0001) {
        QPen fg(QColor("#237653"), 14, Qt::SolidLine, Qt::RoundCap);
        p.setPen(fg);
        const int span = int(qBound(0.0, m_progress, 1.0) * 360 * 16);
        p.drawArc(ringRect, 90 * 16, -span);
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
// 预约凭证(电子票券)自绘部件
// 设计语言: 登机牌/票根 —— 顶部墨绿渐变色带、齿孔虚线与两侧半圆撕口、
// 浅底存根, 搭配细线呼吸状态标记; 用于预约等待视图。
// ============================================================================
namespace VoucherUi {
constexpr qreal kRadius = 16.0;

QPainterPath topRoundedRect(const QRectF &r, qreal rad)
{
    QPainterPath p;
    p.moveTo(r.left(), r.bottom());
    p.lineTo(r.left(), r.top() + rad);
    p.arcTo(QRectF(r.left(), r.top(), rad * 2, rad * 2), 180, -90);
    p.lineTo(r.right() - rad, r.top());
    p.arcTo(QRectF(r.right() - rad * 2, r.top(), rad * 2, rad * 2), 90, -90);
    p.lineTo(r.right(), r.bottom());
    p.closeSubpath();
    return p;
}

QPainterPath bottomRoundedRect(const QRectF &r, qreal rad)
{
    QPainterPath p;
    p.moveTo(r.left(), r.top());
    p.lineTo(r.right(), r.top());
    p.lineTo(r.right(), r.bottom() - rad);
    p.arcTo(QRectF(r.right() - rad * 2, r.bottom() - rad * 2, rad * 2, rad * 2), 0, -90);
    p.lineTo(r.left() + rad, r.bottom());
    p.arcTo(QRectF(r.left(), r.bottom() - rad * 2, rad * 2, rad * 2), 270, -90);
    p.lineTo(r.left(), r.top());
    p.closeSubpath();
    return p;
}

// 色带描边: 仅左竖边 -> 左上圆角 -> 顶边 -> 右上圆角 -> 右竖边(不画底边横线)
QPainterPath topStrokePath(const QRectF &r, qreal rad)
{
    QPainterPath p;
    p.moveTo(r.left(), r.bottom());
    p.lineTo(r.left(), r.top() + rad);
    p.arcTo(QRectF(r.left(), r.top(), rad * 2, rad * 2), 180, -90);
    p.lineTo(r.right() - rad, r.top());
    p.arcTo(QRectF(r.right() - rad * 2, r.top(), rad * 2, rad * 2), 90, -90);
    p.lineTo(r.right(), r.bottom());
    return p;
}

// 存根描边: 仅左竖边 -> 左下圆角 -> 底边 -> 右下圆角 -> 右竖边(不画顶边横线)
QPainterPath bottomStrokePath(const QRectF &r, qreal rad)
{
    QPainterPath p;
    p.moveTo(r.left(), r.top());
    p.lineTo(r.left(), r.bottom() - rad);
    p.arcTo(QRectF(r.left(), r.bottom() - rad * 2, rad * 2, rad * 2), 180, 90);
    p.lineTo(r.right() - rad, r.bottom());
    p.arcTo(QRectF(r.right() - rad * 2, r.bottom() - rad * 2, rad * 2, rad * 2), 270, 90);
    p.lineTo(r.right(), r.top());
    return p;
}
} // namespace VoucherUi

// 顶部品牌色带: 墨绿纵向渐变, 仅上方圆角
class VoucherBand : public QWidget
{
public:
    explicit VoucherBand(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedHeight(72);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0.0, QColor("#1D5C3F"));
        g.setColorAt(1.0, QColor("#309168"));
        p.setBrush(g);
        p.drawPath(VoucherUi::topRoundedRect(rect(), VoucherUi::kRadius));
        QPen edge(QColor("#1A5739"));
        edge.setWidthF(1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(edge);
        p.drawPath(VoucherUi::topStrokePath(
            QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), VoucherUi::kRadius));
    }
};

// 齿孔撕口: 上半承接白色主体、下半承接浅底存根, 两侧半圆缺口 + 居中虚线
class VoucherPerforation : public QWidget
{
public:
    explicit VoucherPerforation(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedHeight(20);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#FFFFFF"));
        p.drawRect(QRectF(0, 0, width(), height() / 2.0));
        p.setBrush(QColor("#F5F9F6"));
        p.drawRect(QRectF(0, height() / 2.0, width(), height() / 2.0));
        // 两侧半圆缺口, 取页面窗口色, 形成票根撕口
        p.setBrush(palette().color(QPalette::Window));
        const qreal rr = height() / 2.0;
        p.drawEllipse(QPointF(0, height() / 2.0), rr, rr);
        p.drawEllipse(QPointF(width(), height() / 2.0), rr, rr);
        QPen dash(QColor("#C4CFC7"));
        dash.setWidthF(1.5);
        dash.setCapStyle(Qt::RoundCap);
        dash.setDashPattern(QVector<qreal>{3.0, 4.0});
        p.setPen(dash);
        p.drawLine(QPointF(20, height() / 2.0), QPointF(width() - 20, height() / 2.0));
    }
};

// 底部存根: 极浅绿灰底, 仅下方圆角
class VoucherStub : public QWidget
{
public:
    explicit VoucherStub(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#F5F9F6"));
        p.drawPath(VoucherUi::bottomRoundedRect(rect(), VoucherUi::kRadius));
        QPen edge(QColor("#DFE8E1"));
        edge.setWidthF(1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(edge);
        p.drawPath(VoucherUi::bottomStrokePath(
            QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), VoucherUi::kRadius));
    }
};

// 时段连接器: 虚线贯穿, 中央墨绿圆底白色闪电
class TimeConnector : public QWidget
{
public:
    explicit TimeConnector(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(92, 32);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal cy = height() / 2.0;
        QPen dash(QColor("#AFC4B8"));
        dash.setWidthF(1.4);
        dash.setCapStyle(Qt::RoundCap);
        dash.setDashPattern(QVector<qreal>{2.5, 4.0});
        p.setPen(dash);
        p.drawLine(QPointF(2, cy), QPointF(width() - 2, cy));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#237653"));
        p.drawEllipse(QPointF(width() / 2.0, cy), 12.5, 12.5);
        const QPixmap bolt = IconFactory::icon(IconFactory::IconBolt, QColor("#FFFFFF"), 18).pixmap(18, 18);
        p.drawPixmap(int(width() / 2.0 - 9), int(cy - 9), bolt);
    }
};

// 状态标记: 细线圆环 + 对勾, 外环缓慢呼吸扩散
class WaitingStatusMark : public QWidget
{
public:
    enum Kind { Appoint = 0 };
    explicit WaitingStatusMark(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(56, 56);
        m_anim.setDuration(2300);
        m_anim.setStartValue(0.0);
        m_anim.setEndValue(1.0);
        m_anim.setLoopCount(-1);
        m_anim.setEasingCurve(QEasingCurve::Linear);
        connect(&m_anim, &QVariantAnimation::valueChanged, this, [this] { update(); });
        m_anim.start();
    }
    void setKind(Kind k) { m_kind = k; update(); }
protected:
    void showEvent(QShowEvent *) override
    {
        if (m_anim.state() != QAbstractAnimation::Running)
            m_anim.resume();
    }
    void hideEvent(QHideEvent *) override { m_anim.pause(); }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor main("#237653");
        const qreal cx = width() / 2.0, cy = height() / 2.0;

        const qreal t = m_anim.currentValue().toReal();
        QColor halo = main;
        halo.setAlphaF(0.20 * (1.0 - t));
        QPen hp(halo);
        hp.setWidthF(2.0);
        hp.setCapStyle(Qt::RoundCap);
        p.setPen(hp);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(cx, cy), 16.0 + t * 10.0, 16.0 + t * 10.0);

        QPen ring(main);
        ring.setWidthF(2.2);
        ring.setCapStyle(Qt::RoundCap);
        p.setPen(ring);
        p.drawEllipse(QPointF(cx, cy), 15.0, 15.0);

        QPen ic(main);
        ic.setWidthF(2.3);
        ic.setCapStyle(Qt::RoundCap);
        ic.setJoinStyle(Qt::RoundJoin);
        p.setPen(ic);
        QPainterPath ck;
        ck.moveTo(cx - 6.5, cy + 0.8);
        ck.lineTo(cx - 1.6, cy + 5.6);
        ck.lineTo(cx + 7.0, cy - 5.2);
        p.drawPath(ck);
    }
private:
    QVariantAnimation m_anim;
    Kind m_kind = Appoint;
};

// ============================================================================
// 充电设置对话框
// ============================================================================
class ChargeSetupDialog : public QDialog
{
public:
    ChargeSetupDialog(const PileInfo &pile, double basePrice, QWidget *parent)
        : QDialog(parent)
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
        auto *billingHint = new QLabel(
            QStringLiteral("费用按实际充电量从余额实时扣除，余额用完后自动停止"), this);
        billingHint->setObjectName("pageHint");
        billingHint->setWordWrap(true);

        form->addRow(pileLabel);
        form->addRow(QStringLiteral("充电目标:"), m_type);
        form->addRow(QStringLiteral("目标数值:"), m_value);
        form->addRow(priceLabel);
        form->addRow(QStringLiteral("计费方式:"), billingHint);

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
    }

    QComboBox *m_type;
    QDoubleSpinBox *m_value;
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
                    "border-radius:6px;} QPushButton:checked{background:#237653;color:white;"
                    "border:1px solid #237653;font-weight:bold;}");
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
                b->setStyleSheet("QPushButton{background:#237653;color:white;border:1px solid #237653;"
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
        "QPushButton#primaryBtn{background:#237653;color:white;border:none;border-radius:8px;"
        "padding:7px 14px;font-weight:bold;} QPushButton#primaryBtn:hover{background:#1B5BB8;}"
        "QPushButton#ghostBtn{background:white;color:#237653;border:1px solid #237653;border-radius:8px;"
        "padding:6px 12px;} QPushButton#ghostBtn:hover{background:#EEF8F0;}"
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

    QLabel *title = new QLabel(QStringLiteral("选择电桩，开启充电"), m_selectView);
    title->setObjectName("pageTitle");
    QLabel *hint = new QLabel(
        QStringLiteral("选择充电站与电桩：空闲桩可立即充电，所有正常电桩均可预约时段"), m_selectView);
    hint->setObjectName("pageHint");

    QHBoxLayout *searchRow = new QHBoxLayout();
    m_stationSearch = new QLineEdit(m_selectView);
    m_stationSearch->setObjectName("chargingStationSearch");
    m_stationSearch->setClearButtonEnabled(true);
    m_stationSearch->setPlaceholderText(QStringLiteral("搜索充电站名称或地址"));
    m_stationSearch->setAccessibleName(QStringLiteral("搜索充电站"));
    QPushButton *searchBtn = new QPushButton(QStringLiteral("搜索"), m_selectView);
    searchBtn->setObjectName("chargingStationSearchButton");
    searchRow->addWidget(m_stationSearch, 1);
    searchRow->addWidget(searchBtn);

    QHBoxLayout *stationRow = new QHBoxLayout();
    stationRow->addWidget(new QLabel(QStringLiteral("充电站:"), m_selectView));
    m_stationCombo = new QComboBox(m_selectView);
    m_stationCombo->setObjectName("stationCombo");
    m_stationCombo->setMinimumWidth(180);
    m_stationCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), m_selectView);
    refreshBtn->setObjectName("searchButton");
    stationRow->addWidget(m_stationCombo, 1);
    stationRow->addWidget(refreshBtn);
    m_stationInfo = new QLabel(m_selectView);
    m_stationInfo->setStyleSheet("color:#6B7280;");
    stationRow->addWidget(m_stationInfo);

    m_cardScroll = new QScrollArea(m_selectView);
    m_cardScroll->setWidgetResizable(true);
    m_cardScroll->setFrameShape(QFrame::NoFrame);
    m_cardHost = new QWidget(m_cardScroll);
    m_cardHost->setObjectName("chargingCardHost");
    m_cardGrid = new QGridLayout(m_cardHost);
    m_cardGrid->setContentsMargins(2, 2, 2, 2);
    m_cardGrid->setSpacing(12);
    m_cardGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_cardScroll->setWidget(m_cardHost);

    lay->addWidget(title);
    lay->addWidget(hint);
    lay->addLayout(searchRow);
    lay->addLayout(stationRow);
    lay->addWidget(m_cardScroll, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &ChargingPage::refreshStations);
    auto *searchDelay = new QTimer(this);
    searchDelay->setSingleShot(true);
    searchDelay->setInterval(250);
    connect(m_stationSearch, &QLineEdit::textChanged, this,
            [searchDelay] { searchDelay->start(); });
    connect(searchDelay, &QTimer::timeout, this, [this] { applyStationFilter(); });
    connect(searchBtn, &QPushButton::clicked, this, [this, searchDelay] {
        searchDelay->stop();
        applyStationFilter();
    });
    connect(m_stationSearch, &QLineEdit::returnPressed, searchBtn, &QPushButton::click);
    connect(m_stationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onStationPicked(); });
}

void ChargingPage::buildChargingView()
{
    m_chargingView = new ChargingParticles(this);
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

    // 实时充电曲线
    auto *chartRow = new QHBoxLayout();
    chartRow->setSpacing(8);
    auto *chartTitle = new QLabel(QStringLiteral("充电曲线 · 演示模拟"), m_chargingView);
    chartTitle->setStyleSheet("font-size:13px;font-weight:bold;color:#1A1B1C;");
    m_chartModeBtn = new QPushButton(QStringLiteral("切换:电量"), m_chargingView);
    m_chartModeBtn->setStyleSheet("QPushButton{background:#EDF7F0;border:1px solid #91BFA0;"
                                   "border-radius:6px;padding:4px 12px;color:#237653;font-size:11px;}"
                                   "QPushButton:hover{background:#E2F2E7;}");
    m_chartModeBtn->setCursor(Qt::PointingHandCursor);
    chartRow->addWidget(chartTitle);
    chartRow->addStretch();
    chartRow->addWidget(m_chartModeBtn);

    m_chart = new ChargeChartWidget(m_chargingView);
    m_chart->setFixedHeight(160);

    QPushButton *stopBtn = new QPushButton(QStringLiteral("结束充电并结算"), m_chargingView);
    stopBtn->setObjectName("settleBtn");
    stopBtn->setCursor(Qt::PointingHandCursor);

    lay->addWidget(title);
    lay->addWidget(m_orderTitle);
    lay->addWidget(m_ring, 0, Qt::AlignHCenter);
    lay->addLayout(cards);
    lay->addWidget(m_priceHint);
    lay->addSpacing(4);
    lay->addLayout(chartRow);
    lay->addWidget(m_chart);
    lay->addStretch();
    lay->addWidget(stopBtn);

    connect(stopBtn, &QPushButton::clicked, this, &ChargingPage::onStopCharge);
    connect(m_chartModeBtn, &QPushButton::clicked, this, [this]() {
        const int next = (m_chart->mode() + 1) % 3;
        m_chart->setMode(next);
        const QStringList labels{"切换:金额", "切换:功率", "切换:电量"};
        m_chartModeBtn->setText(labels[next]);
    });
}

void ChargingPage::buildWaitingView()
{
    m_waitingView = new QWidget(this);
    m_waitingView->setStyleSheet(QStringLiteral(
        "QLabel{background:transparent;}"
        "QPushButton#voucherCancel{background:#FFFFFF;color:#67736B;border:1px solid #D5DED8;"
        "border-radius:10px;font-size:13px;}"
        "QPushButton#voucherCancel:hover{background:#FDF1F1;color:#C5525A;border:1px solid #E6C2C5;}"
        "QPushButton#voucherCancel:pressed{background:#F9E3E4;border:1px solid #E0B3B7;}"));

    QVBoxLayout *page = new QVBoxLayout(m_waitingView);
    page->setContentsMargins(24, 16, 24, 24);
    page->setSpacing(0);
    page->addStretch();

    // 单层固定宽度卡片; 不使用 QGraphicsEffect, 避免与全局交互动效叠加时产生错位/漏底
    m_voucherCard = new QWidget(m_waitingView);
    m_voucherCard->setFixedWidth(440);
    QVBoxLayout *card = new QVBoxLayout(m_voucherCard);
    card->setContentsMargins(0, 0, 0, 0);
    card->setSpacing(0);

    // —— 顶部色带 ——
    auto *band = new VoucherBand(m_voucherCard);
    QHBoxLayout *bandLay = new QHBoxLayout(band);
    bandLay->setContentsMargins(24, 0, 22, 0);
    bandLay->setSpacing(10);
    auto *bolt = new QLabel(band);
    bolt->setPixmap(IconFactory::icon(IconFactory::IconBolt, QColor("#FFFFFF"), 22).pixmap(22, 22));
    auto *brand = new QLabel(QStringLiteral("东软充电"), band);
    QFont bf = brand->font();
    bf.setPointSize(13);
    bf.setBold(true);
    brand->setFont(bf);
    brand->setStyleSheet("color:#FFFFFF;font-size:14px;font-weight:700;");
    auto *bandSep = new QFrame(band);
    bandSep->setFixedSize(1, 22);
    bandSep->setStyleSheet("background:rgba(255,255,255,0.35);");
    m_bandTitle = new QLabel(band);
    m_bandTitle->setStyleSheet("color:rgba(255,255,255,0.85);font-size:11px;");
    m_bandEn = new QLabel(band);
    QFont enTop = m_bandEn->font();
    enTop.setPointSize(8);
    enTop.setLetterSpacing(QFont::AbsoluteSpacing, 2.6);
    m_bandEn->setFont(enTop);
    m_bandEn->setStyleSheet("color:rgba(255,255,255,0.62);font-size:9px;");
    bandLay->addWidget(bolt);
    bandLay->addWidget(brand);
    bandLay->addSpacing(6);
    bandLay->addWidget(bandSep);
    bandLay->addSpacing(4);
    bandLay->addWidget(m_bandTitle);
    bandLay->addStretch();
    bandLay->addWidget(m_bandEn);

    // —— 白色主体 ——
    auto *body = new QWidget(m_voucherCard);
    body->setObjectName(QStringLiteral("voucherBody"));
    body->setStyleSheet(QStringLiteral(
        "#voucherBody{background:#FFFFFF;border-left:1px solid #DFE8E1;"
        "border-right:1px solid #DFE8E1;}"));
    QVBoxLayout *bl = new QVBoxLayout(body);
    bl->setContentsMargins(34, 24, 34, 24);
    bl->setSpacing(12);

    // 状态行: 呼吸标记 + 主/副标题
    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(12);
    m_waitMark = new WaitingStatusMark(body);
    auto *statusText = new QVBoxLayout();
    statusText->setSpacing(2);
    m_waitStatusTitle = new QLabel(body);
    QFont stf = m_waitStatusTitle->font();
    stf.setPointSize(15);
    stf.setBold(true);
    m_waitStatusTitle->setFont(stf);
    m_waitStatusTitle->setStyleSheet("color:#18263D;font-size:16px;font-weight:700;");
    m_waitStatusEn = new QLabel(body);
    QFont sef = m_waitStatusEn->font();
    sef.setPointSize(8);
    sef.setLetterSpacing(QFont::AbsoluteSpacing, 2.2);
    m_waitStatusEn->setFont(sef);
    m_waitStatusEn->setStyleSheet("color:#9AA7B2;font-size:9px;");
    statusText->addStretch();
    statusText->addWidget(m_waitStatusTitle);
    statusText->addWidget(m_waitStatusEn);
    statusText->addStretch();
    statusRow->addStretch();
    statusRow->addWidget(m_waitMark);
    statusRow->addLayout(statusText);
    statusRow->addStretch();
    bl->addLayout(statusRow);

    bl->addSpacing(4);
    m_waitPileCode = new QLabel(body);
    QFont pf = m_waitPileCode->font();
    pf.setPointSize(19);
    pf.setBold(true);
    m_waitPileCode->setFont(pf);
    m_waitPileCode->setAlignment(Qt::AlignCenter);
    m_waitPileCode->setStyleSheet("color:#13231A;font-size:21px;font-weight:700;");
    m_waitStation = new QLabel(body);
    m_waitStation->setAlignment(Qt::AlignCenter);
    m_waitStation->setStyleSheet("color:#8493A0;font-size:11px;");
    bl->addWidget(m_waitPileCode);
    bl->addWidget(m_waitStation);

    // 时段核心(预约)
    m_appointCore = new QWidget(body);
    m_appointCore->setObjectName(QStringLiteral("appointCore"));
    m_appointCore->setStyleSheet(QStringLiteral("#appointCore{background:transparent;}"));
    auto *ap = new QVBoxLayout(m_appointCore);
    ap->setContentsMargins(0, 6, 0, 2);
    ap->setSpacing(14);
    m_waitDate = new QLabel(m_appointCore);
    m_waitDate->setAlignment(Qt::AlignCenter);
    m_waitDate->setFixedHeight(28);
    m_waitDate->setStyleSheet("background:#EAF4EE;color:#237653;border-radius:9px;"
                              "padding:4px 16px;font-size:11px;font-weight:bold;");
    auto *dateRow = new QHBoxLayout();
    dateRow->addStretch();
    dateRow->addWidget(m_waitDate);
    dateRow->addStretch();
    auto *timeRow = new QHBoxLayout();
    timeRow->setSpacing(0);
    auto makeTimeCol = [body](const QString &cap, QLabel **out) -> QVBoxLayout * {
        auto *c = new QVBoxLayout();
        c->setSpacing(4);
        auto *v = new QLabel(QStringLiteral("--:--"), body);
        v->setAlignment(Qt::AlignCenter);
        QFont vf = v->font();
        vf.setPointSize(25);
        vf.setBold(true);
        v->setFont(vf);
        v->setStyleSheet("color:#18263D;font-size:27px;font-weight:700;");
        auto *cp = new QLabel(cap, body);
        cp->setAlignment(Qt::AlignCenter);
        QFont cf = cp->font();
        cf.setPointSize(8);
        cf.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
        cp->setFont(cf);
        cp->setStyleSheet("color:#9AA6B2;font-size:9px;");
        c->addWidget(v);
        c->addWidget(cp);
        *out = v;
        return c;
    };
    timeRow->addLayout(makeTimeCol(QStringLiteral("开始  START"), &m_waitStart), 1);
    timeRow->addWidget(new TimeConnector(body), 0, Qt::AlignVCenter);
    timeRow->addLayout(makeTimeCol(QStringLiteral("结束  END"), &m_waitEnd), 1);
    ap->addLayout(dateRow);
    ap->addLayout(timeRow);
    bl->addWidget(m_appointCore);

    card->addWidget(band);
    card->addWidget(body);
    card->addWidget(new VoucherPerforation(m_voucherCard));

    // —— 底部存根 ——
    auto *stub = new VoucherStub(m_voucherCard);
    auto *sl = new QVBoxLayout(stub);
    sl->setContentsMargins(30, 16, 30, 20);
    sl->setSpacing(12);
    auto *tipRow = new QHBoxLayout();
    tipRow->setSpacing(10);
    auto *accentBar = new QFrame(stub);
    accentBar->setFixedSize(3, 30);
    accentBar->setStyleSheet("background:#237653;border-radius:1.5px;");
    m_waitTip = new QLabel(stub);
    m_waitTip->setWordWrap(true);
    m_waitTip->setStyleSheet("color:#56665C;font-size:12px;");
    tipRow->addWidget(accentBar, 0, Qt::AlignVCenter);
    tipRow->addWidget(m_waitTip, 1);
    sl->addLayout(tipRow);
    m_waitVoucherNo = new QLabel(stub);
    m_waitVoucherNo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont nf = m_waitVoucherNo->font();
    nf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    m_waitVoucherNo->setFont(nf);
    m_waitVoucherNo->setStyleSheet("color:#A9B5AE;font-size:10px;");
    sl->addWidget(m_waitVoucherNo);
    m_cancelWaitBtn = new QPushButton(stub);
    m_cancelWaitBtn->setObjectName("voucherCancel");
    m_cancelWaitBtn->setCursor(Qt::PointingHandCursor);
    m_cancelWaitBtn->setFixedSize(212, 40);
    sl->addWidget(m_cancelWaitBtn, 0, Qt::AlignHCenter);
    card->addWidget(stub);

    page->addWidget(m_voucherCard, 0, Qt::AlignHCenter);
    page->addStretch();

    connect(m_cancelWaitBtn, &QPushButton::clicked, this, &ChargingPage::onCancelWaiting);
}

void ChargingPage::selectStation(int stationId)
{
    m_requestedStationId = stationId;
    if (m_stationSearch) {
        const QSignalBlocker blocker(m_stationSearch);
        m_stationSearch->clear();
    }
    if (isVisible() && !m_hasOrder && m_waitingId < 0)
        refreshStations();
}

void ChargingPage::refreshStations()
{
    const int selectedId = m_requestedStationId >= 0 ? m_requestedStationId : m_stationCombo->currentData().toInt();
    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationList, QJsonObject{{"lon", 123.45}, {"lat", 41.70}});
    if (!reply.value("ok").toBool()) {
        if (m_silentRefresh)
            m_stationInfo->setText(QStringLiteral("自动刷新失败，将稍后重试"));
        else
            QMessageBox::warning(this, QStringLiteral("查询失败"), reply.value("error").toString());
        return;
    }
    m_stations.clear();
    const QJsonArray arr = reply.value("stations").toArray();
    for (const QJsonValue &v : arr)
        m_stations.append(StationInfo::fromJson(v.toObject()));

    const bool requestedStationMissing = m_requestedStationId >= 0
        && findStation(m_stations, selectedId).id <= 0;
    m_requestedStationId = -1;
    applyStationFilter(selectedId);
    if (requestedStationMissing) {
        QMessageBox::information(this, "站点已更新", "所选站点已不可用，请返回首页刷新或选择其他站点。");
    }
}

void ChargingPage::applyStationFilter(int preferredStationId)
{
    if (!m_stationCombo || !m_stationSearch)
        return;
    if (preferredStationId < 0)
        preferredStationId = m_stationCombo->currentData().toInt();
    const QString keyword = m_stationSearch->text().trimmed();

    m_stationCombo->blockSignals(true);
    m_stationCombo->clear();
    for (const StationInfo &station : m_stations) {
        if (!keyword.isEmpty()
            && !station.name.contains(keyword, Qt::CaseInsensitive)
            && !station.address.contains(keyword, Qt::CaseInsensitive))
            continue;
        m_stationCombo->addItem(QString("%1 (空闲 %2/%3)")
                                    .arg(station.name).arg(station.idlePiles).arg(station.totalPiles),
                                station.id);
    }
    const int preferredIndex = m_stationCombo->findData(preferredStationId);
    if (preferredIndex >= 0)
        m_stationCombo->setCurrentIndex(preferredIndex);
    else if (m_stationCombo->count() > 0)
        m_stationCombo->setCurrentIndex(0);
    else
        m_stationCombo->setCurrentIndex(-1);
    m_stationCombo->blockSignals(false);

    if (m_stationCombo->count() == 0) {
        m_stationInfo->setText(QStringLiteral("没有找到匹配的充电站"));
        m_piles.clear();
        rebuildPileCards();
        return;
    }
    onStationPicked();
}

void ChargingPage::onStationPicked()
{
    m_piles.clear();
    const StationInfo s = findStation(m_stations, m_stationCombo->currentData().toInt());
    if (s.id <= 0) {
        rebuildPileCards();
        return;
    }
    m_stationInfo->setText(QStringLiteral("基准电价 %1 元/度").arg(s.price, 0, 'f', 2));

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqStationPiles, QJsonObject{{"stationId", s.id}});
    if (reply.value("ok").toBool()) {
        const QJsonArray piles = reply.value("piles").toArray();
        for (const QJsonValue &v : piles)
            m_piles.append(PileInfo::fromJson(v.toObject()));
    }
    if (!reply.value("ok").toBool())
        m_stationInfo->setText("电桩加载失败，请点击刷新重试");
    rebuildPileCards();
}

void ChargingPage::rebuildPileCards()
{
    QLayoutItem *it = nullptr;
    while ((it = m_cardGrid->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    if (m_piles.isEmpty()) {
        auto *empty = new QLabel("暂无可选电桩，请刷新或选择其他站点", m_cardHost);
        empty->setObjectName("emptyState");
        m_cardGrid->addWidget(empty, 0, 0);
    }
    const int columns = qBound(1, (m_cardScroll->viewport()->width() - 12) / 232, 3);
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
            QPushButton *appoint = new QPushButton(QStringLiteral("预约时段"), card);
            appoint->setObjectName("ghostBtn");
            appoint->setCursor(Qt::PointingHandCursor);
            connect(appoint, &QPushButton::clicked, this, [this, id = p.id] { openAppointDialog(id); });
            btns->addWidget(appoint);
            btns->addStretch();
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
    const StationInfo station = findStation(m_stations, m_stationCombo->currentData().toInt());
    const double basePrice = station.id > 0 ? station.price : 1.2;
    ChargeSetupDialog dlg(findPile(m_piles, pileId), basePrice, this);
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
    if (QMessageBox::question(this, QStringLiteral("取消预约"),
                              QStringLiteral("确定取消该预约吗?")) != QMessageBox::Yes)
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
    const bool sameOrder = m_hasOrder && m_currentOrder.id == order.id;
    m_currentOrder = order;
    m_hasOrder = true;
    m_orderTitle->setText(QStringLiteral("订单 #%1    %2    电桩 %3")
                              .arg(order.id).arg(order.stationName, order.pileCode));
    m_energyVal->setText(QString::number(order.energy, 'f', 2));
    m_amountVal->setText(QString::number(order.amount, 'f', 2));
    m_minutesVal->setText(QString::number(order.simMinutes));
    m_ring->setCenterText(QString::number(order.energy, 'f', 1), QStringLiteral("度"));
    // 有明确目标时展示目标完成度；手动结束模式按时长持续增长并逐渐接近满环。
    double progress = 1.0 - qExp(-qMax(0, order.simMinutes) / 30.0);
    if (order.targetValue > 0) {
        if (order.targetType == TargetEnergy)
            progress = order.energy / order.targetValue;
        else if (order.targetType == TargetAmount)
            progress = order.amount / order.targetValue;
        else if (order.targetType == TargetMinutes)
            progress = double(order.simMinutes) / order.targetValue;
    }
    m_ring->setProgress(progress);
    if (m_chart && !sameOrder) {
        m_chart->clearData();
        if (order.simMinutes > 0)
            m_chart->addPoint(order.simMinutes, order.energy, order.amount);
    }
    m_priceHint->setText(QStringLiteral("单价 %1 元/度 · 费用实时扣除 · %2")
                             .arg(order.priceSnapshot, 0, 'f', 2)
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
    if (r.type != ReserveAppoint)
        return;
    m_waitingId = r.id;

    auto *mark = static_cast<WaitingStatusMark *>(m_waitMark);
    m_waitPileCode->setText(r.pileCode);
    if (r.stationName.trimmed().isEmpty())
        m_waitStation->hide();
    else {
        m_waitStation->setText(r.stationName);
        m_waitStation->show();
    }

    mark->setKind(WaitingStatusMark::Appoint);
    m_bandTitle->setText(QStringLiteral("充电预约凭证"));
    m_bandEn->setText(QStringLiteral("RESERVATION"));
    m_waitStatusTitle->setText(QStringLiteral("时段预约成功"));
    m_waitStatusEn->setText(QStringLiteral("RESERVATION CONFIRMED"));

    QString dateText = r.reserveDate;
    const QDate d = QDate::fromString(r.reserveDate, QStringLiteral("yyyy-MM-dd"));
    if (d.isValid()) {
        const QStringList week = {QStringLiteral("周一"), QStringLiteral("周二"),
                                  QStringLiteral("周三"), QStringLiteral("周四"),
                                  QStringLiteral("周五"), QStringLiteral("周六"),
                                  QStringLiteral("周日")};
        dateText = QStringLiteral("%1  ·  %2").arg(
            r.reserveDate, week.value(d.dayOfWeek() - 1));
    }
    m_waitDate->setText(dateText);
    m_waitStart->setText(r.reserveStart);
    m_waitEnd->setText(r.reserveEnd);
    m_appointCore->show();
    m_waitTip->setText(QStringLiteral("开始前 10 分钟将推送提醒，请按时到场扫码启动充电"));
    m_waitVoucherNo->setText(
        QStringLiteral("预约编号  NO.%1").arg(r.id, 6, 10, QChar('0')));
    m_cancelWaitBtn->setText(QStringLiteral("取消预约"));

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
        if (r.type == ReserveAppoint
            && (r.status == ReservationActive || r.status == ReservationAssigned)) {
            enterWaitingView(r);
            return;
        }
    }
}

void ChargingPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshPage();
}

void ChargingPage::refreshPage()
{
    if (!isVisible() || TcpClient::instance().isBusy())
        return;

    m_silentRefresh = true;

    const QJsonObject reply = TcpClient::instance().request(
        Protocol::ReqUnfinishedOrder,
        QJsonObject{{"userId", ClientSession::instance().userId}});
    if (!reply.value("ok").toBool()) {
        m_silentRefresh = false;
        return;
    }
    if (reply.value("ok").toBool() && reply.value("hasOrder").toBool()) {
        m_currentOrder = OrderInfo::fromJson(reply.value("order").toObject());
        m_requestedStationId = -1;
        enterChargingView(m_currentOrder);
        m_silentRefresh = false;
        return;
    }

    // 无在充订单: 检查是否有有效预约
    const QJsonObject res = TcpClient::instance().request(
        Protocol::ReqMyReservations,
        QJsonObject{{"userId", ClientSession::instance().userId}});
    if (!res.value("ok").toBool()) {
        m_silentRefresh = false;
        return;
    }
    bool waiting = false;
    if (res.value("ok").toBool()) {
        const QJsonArray arr = res.value("reservations").toArray();
        for (const QJsonValue &v : arr) {
            const ReservationInfo r = ReservationInfo::fromJson(v.toObject());
            if (r.type == ReserveAppoint
                && (r.status == ReservationActive || r.status == ReservationAssigned)) {
                m_requestedStationId = -1;
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
    m_silentRefresh = false;
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
        if (msg.contains("balance"))
            ClientSession::instance().balance = msg.value("balance").toDouble();
        m_energyVal->setText(QString::number(m_currentOrder.energy, 'f', 2));
        m_amountVal->setText(QString::number(m_currentOrder.amount, 'f', 2));
        m_minutesVal->setText(QString::number(m_currentOrder.simMinutes));
        m_ring->setCenterText(QString::number(m_currentOrder.energy, 'f', 1),
                              QStringLiteral("度"));
        if (m_currentOrder.targetType == TargetNone) {
            m_ring->setProgress(1.0 - qExp(-qMax(0, m_currentOrder.simMinutes) / 30.0));
        } else if (msg.contains("targetProgress")) {
            m_ring->setProgress(msg.value("targetProgress").toDouble());
        }
        if (m_chart)
            m_chart->addPoint(m_currentOrder.simMinutes, m_currentOrder.energy, m_currentOrder.amount,
                              msg.value("power").toDouble(-1));
        return;
    }

    if (type != Protocol::PushOrderEvent)
        return;

    const int event = msg.value("event").toInt();
    if (event == 1 || event == 4) {
        return; // 忽略旧服务端可能发送的已下线功能事件。
    } else if (event == 7
               && msg.value("message").toString().contains(QStringLiteral("排队"))) {
        return;
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
                             QStringLiteral("订单 #%1 因故障中断，已按实际用量结算\n"
                                            "消费 %2 元, 如有疑问可联系管理员退款")
                                 .arg(order.id).arg(order.amount, 0, 'f', 2));
        enterSelectView();
        refreshStations();
    } else if (event == 6) {
        QMessageBox::information(this, QStringLiteral("预约提醒"),
                                 QStringLiteral("您预约的电桩 %1 将在 10 分钟后开放, 请准备到场")
                                     .arg(msg.value("pileCode").toString()));
    } else if (event == 7) {
        QMessageBox::information(this, QStringLiteral("预约通知"),
                                 msg.value("message").toString(QStringLiteral("您的预约状态已更新")));
        if (m_stack->currentIndex() == 2) {
            m_waitingId = -1;
            enterSelectView();
            refreshStations();
        }
    } else if (event == 8) {
        // 退款到账通知: 更新余额并提示
        const double refundAmount = msg.value("refundAmount").toDouble();
        const int orderId = msg.value("orderId").toInt();
        const QJsonObject info = TcpClient::instance().request(Protocol::ReqGetUserInfo, QJsonObject{});
        if (info.value("ok").toBool())
            ClientSession::instance().balance = info.value("balance").toDouble();
        QMessageBox::information(this, QStringLiteral("退款到账"),
                                 QStringLiteral("订单 #%1 的退款 %2 元已到账\n当前余额: %3 元")
                                     .arg(orderId).arg(refundAmount, 0, 'f', 2)
                                     .arg(ClientSession::instance().balance, 0, 'f', 2));
    }
}

void ChargingPage::showSettlement(const OrderInfo &order, double balance)
{
    ClientSession::instance().balance = balance;
    m_hasOrder = false;

    QString reason;
    if (order.finishType == FinishByTarget) reason = QStringLiteral("已达到设定目标, 自动结束");
    else if (order.finishType == FinishByBalance) reason = QStringLiteral("余额用尽，自动结束");
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
