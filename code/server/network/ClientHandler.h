#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>

// 单个客户端连接的处理类, 运行在独立工作线程中:
//   套接字读写 / JSON 解析 / 业务处理(含数据库访问)全部在本线程完成
// 数据库: 每个线程创建独立的 QSqlDatabase 连接(QSqlDatabase 连接禁止跨线程共用),
//         连接名以线程 id 区分, 查询时通过 connName 参数传给 Dao
// 充电推进: v2 起统一收归主线程 ChargingEngine, 本类只负责发起/结算/排队/预约等请求,
//           客户端断线不再影响充电; 引擎通过 pushToClient() 跨线程向本连接推送
class ClientHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientHandler() override;

signals:
    void finished();   // 连接结束, 通知所属线程退出

public slots:
    void start();          // 在工作线程中执行: 初始化套接字与数据库连接
    void onReadyRead();
    void onDisconnected();
    // 供 ChargingEngine 在主线程通过队列连接调用, 向该连接推送消息
    void pushToClient(const QJsonObject &obj) { sendJson(obj); }

private:
    void processLine(const QByteArray &line);
    void handleRequest(const QJsonObject &request);
    void sendJson(const QJsonObject &obj);
    void sendError(int type, const QString &text);

    // ---- 各请求的业务处理 ----
    QJsonObject processUserLogin(const QJsonObject &req);
    QJsonObject processGetUserInfo(const QJsonObject &req);
    QJsonObject processUpdateProfile(const QJsonObject &req);
    QJsonObject processRecharge(const QJsonObject &req);
    QJsonObject processStationList(const QJsonObject &req);
    QJsonObject processStationPiles(const QJsonObject &req);
    QJsonObject processUnfinishedOrder(const QJsonObject &req);
    QJsonObject startChargeInternal(int replyType, const QJsonObject &req);
    QJsonObject processStopCharge(const QJsonObject &req);
    QJsonObject processReservePile(const QJsonObject &req);     // 现场排队/取消
    QJsonObject processAppointPile(const QJsonObject &req);     // 时段预约
    QJsonObject processAppointSlots(const QJsonObject &req);    // 某日时段占用
    QJsonObject processMyReservations(const QJsonObject &req);  // 我的排队/预约
    QJsonObject processOrderHistory(const QJsonObject &req);    // 订单历史
    QJsonObject processOrderDetail(const QJsonObject &req);     // 订单详情
    QJsonObject processStationFee(const QJsonObject &req);      // 站点分时费率

    qintptr m_descriptor;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_dbConnName;
    int m_userId = -1;      // 登录后绑定的用户
};

#endif // CLIENTHANDLER_H
