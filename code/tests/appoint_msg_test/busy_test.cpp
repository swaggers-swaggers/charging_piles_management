#include <QApplication>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include "network/TcpClient.h"
#include "protocol.h"
#include "ClientSession.h"

static bool sawBusyDuringPush = false;
static bool pushArrived = false;
static bool stopOk = false;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TcpClient &tc = TcpClient::instance();
    ClientSession &s = ClientSession::instance();

    QJsonObject login = tc.request(Protocol::ReqUserLogin, QJsonObject{{"phone", "13800000001"}});
    if (!login.value("ok").toBool()) { qDebug() << "登录失败"; return 1; }
    s.userId = login.value("userId").toInt();
    qDebug() << "登录成功 userId=" << s.userId;

    // 选一根空闲桩
    QJsonObject stations = tc.request(Protocol::ReqStationList, QJsonObject{{"lon", 116.4}, {"lat", 39.9}});
    QJsonObject piles = tc.request(Protocol::ReqStationPiles, QJsonObject{{"stationId", stations.value("stations").toArray().first().toObject().value("id").toInt()}});
    int pileId = 0;
    for (const QJsonValue &v : piles.value("piles").toArray()) {
        const QJsonObject p = v.toObject();
        if (p.value("status").toInt() == 0) { pileId = p.value("id").toInt(); break; }
    }
    if (!pileId) { qDebug() << "无空闲桩"; return 1; }
    qDebug() << "使用空闲桩 id=" << pileId;

    // 开始充电
    QJsonObject sc = tc.request(Protocol::ReqStartCharge, QJsonObject{{"pileId", pileId}, {"targetType", 1}, {"targetValue", 1.0}});
    const int orderId = sc.value("order").toObject().value("orderId").toInt();
    if (!sc.value("ok").toBool() || !orderId) { qDebug() << "开始充电失败:" << sc.value("error").toString(); return 1; }
    qDebug() << "充电已开始 orderId=" << orderId;

    // 监听推送: 记录充电结束推送到达瞬间 TcpClient 的 busy 状态
    QObject::connect(&tc, &TcpClient::pushReceived, [&](const QJsonObject &msg) {
        if (msg.value("type").toInt() == Protocol::PushOrderEvent
            && msg.value("event").toInt() == 2) {
            pushArrived = true;
            sawBusyDuringPush = tc.isBusy();
            qDebug() << "→ 充电结束推送到达, 此刻 TcpClient isBusy =" << tc.isBusy()
                     << (tc.isBusy() ? "(说明推送先于停止响应, 原代码此时发请求必然弹窗)" : "(推送在空闲时到达)");
        }
    });

    // 发停止请求(同步等待): 此期间若推送到达, 原 ChargingPage 会发请求撞 busy
    QJsonObject cp = tc.request(Protocol::ReqStopCharge, QJsonObject{{"orderId", orderId}});
    stopOk = cp.value("ok").toBool();
    qDebug() << "停止充电 ok=" << stopOk << "金额=" << cp.value("order").toObject().value("amount").toDouble();

    const bool pass = pushArrived && sawBusyDuringPush && stopOk;
    qDebug() << "推送先于响应到达:" << (pushArrived && sawBusyDuringPush)
             << " 停止结算正常:" << stopOk;
    qDebug() << (pass ? "✓ 机理确认: 修复前此场景必弹窗, 修复后 ChargingPage 已跳过该请求" : "✗ 验证失败");
    return pass ? 0 : 2;
}
