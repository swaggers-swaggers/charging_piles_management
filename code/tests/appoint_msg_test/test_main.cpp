#include <QApplication>
#include <QDate>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include "MessageCenter.h"
#include "network/TcpClient.h"
#include "protocol.h"
#include "ClientSession.h"

static bool foundCancel=false, foundEnd=false, foundAppoint=false, foundStart=false;
static QList<int> stopTargets;

void summarize(int code)
{
    const QList<AppMessage> msgs = MessageCenter::instance().messages();
    qDebug() << "── 消息中心共" << msgs.size() << "条 ──";
    for (const AppMessage &m : msgs)
        qDebug() << "  " << m.title << "|" << m.content;
    for (const AppMessage &m : msgs) {
        if (m.title == QStringLiteral("预约成功")) foundAppoint = true;
        if (m.title == QStringLiteral("预约已取消")) foundCancel = true;
        if (m.title == QStringLiteral("充电已开始")) foundStart = true;
        if (m.title == QStringLiteral("充电已结束")) foundEnd = true;
    }
    qDebug() << "预约成功:" << foundAppoint << " 预约取消:" << foundCancel
             << " 充电开始:" << foundStart << " 充电结束:" << foundEnd;
    const bool ok = foundAppoint && foundCancel && foundStart && foundEnd;
    qDebug() << (ok ? "✓ 全部验证通过" : "✗ 存在缺失");
    QCoreApplication::exit(ok ? 0 : 2);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ClientSession &s = ClientSession::instance();
    TcpClient &tc = TcpClient::instance();
    MessageCenter &mc = MessageCenter::instance();

    QJsonObject login = tc.request(Protocol::ReqUserLogin, QJsonObject{{"phone", "13800000001"}});
    if (!login.value("ok").toBool()) { qDebug() << "登录失败:" << login.value("error").toString(); return 1; }
    s.userId = login.value("userId").toInt();
    qDebug() << "登录成功 userId=" << s.userId;

    // 1) 停止所有进行中订单(清残留 + 验证充电结束消息)
    QJsonObject hist = tc.request(Protocol::ReqOrderHistory, QJsonObject{{"userId", s.userId}, {"pageSize", 50}});
    const QJsonArray orders = hist.value("orders").toArray();
    for (const QJsonValue &v : orders) {
        const QJsonObject o = v.toObject();
        if (o.value("status").toInt() == 1)
            stopTargets.append(o.value("orderId").toInt());
    }
    qDebug() << "待停止的进行中订单:" << stopTargets;
    for (int i = 0; i < stopTargets.size(); ++i) {
        QJsonObject r = tc.request(Protocol::ReqStopCharge, QJsonObject{{"orderId", stopTargets[i]}});
        qDebug() << "停止订单" << stopTargets[i] << "ok=" << r.value("ok").toBool();
    }

    // 2) 取消残留预约 rid=14(如存在) + 验证预约取消消息
    const QJsonObject cr = tc.request(Protocol::ReqReservePile, QJsonObject{{"action", 1}, {"reservationId", 14}});
    qDebug() << "取消预约 rid=14 ok=" << cr.value("ok").toBool() << cr.value("error").toString();

    // 3) 预约 CP-055 明天 11:00-12:00
    QJsonObject stations = tc.request(Protocol::ReqStationList, QJsonObject{{"lon", 116.4}, {"lat", 39.9}});
    QJsonObject piles = tc.request(Protocol::ReqStationPiles, QJsonObject{{"stationId", stations.value("stations").toArray().first().toObject().value("id").toInt()}});
    int pileId = 55;
    for (const QJsonValue &v : piles.value("piles").toArray()) {
        const QJsonObject p = v.toObject();
        if (p.value("code").toString() == QStringLiteral("CP-055")) { pileId = p.value("id").toInt(); break; }
    }
    const QString date = QDate::currentDate().addDays(1).toString("yyyy-MM-dd");
    const QJsonObject ar = tc.request(Protocol::ReqAppointPile, QJsonObject{
        {"pileId", pileId}, {"reserveDate", date}, {"reserveStart", "11:00"}, {"reserveEnd", "12:00"}});
    const int rid = ar.value("reservationId").toInt();
    qDebug() << "预约 CP-055 ok=" << ar.value("ok").toBool() << ar.value("error").toString() << "rid=" << rid;

    // 4) 开始充电 CP-055
    QJsonObject sc = tc.request(Protocol::ReqStartCharge, QJsonObject{
        {"pileId", pileId}, {"targetType", 1}, {"targetValue", 1.0}});
    const int orderId = sc.value("order").toObject().value("orderId").toInt();
    qDebug() << "开始充电 ok=" << sc.value("ok").toBool() << sc.value("error").toString() << "orderId=" << orderId;

    // 5) 停止充电
    QTimer::singleShot(600, &app, [&]() {
        QJsonObject cp = tc.request(Protocol::ReqStopCharge, QJsonObject{{"orderId", orderId}});
        qDebug() << "停止充电 ok=" << cp.value("ok").toBool() << cp.value("error").toString()
                 << "金额=" << cp.value("order").toObject().value("amount").toDouble();
    });

    QTimer::singleShot(2600, &app, [&]() { summarize(0); });
    return app.exec();
}
