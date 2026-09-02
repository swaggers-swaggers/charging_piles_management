#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

// 单个客户端连接的处理类, 运行在独立工作线程中:
//   套接字读写 / JSON 解析 / 业务处理(含数据库访问)全部在本线程完成
// 数据库: 每个线程创建独立的 QSqlDatabase 连接(QSqlDatabase 连接禁止跨线程共用),
//         连接名以线程 id 区分, 查询时通过 connName 参数传给 Dao
// 充电模拟: 本连接用户开始充电后启动进度定时器, 每 3 秒真实时间模拟 1 分钟充电,
//           按桩功率累计电量、按站电价累计金额, 并向客户端推送进度
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
    void onProgressTick(); // 充电进度模拟定时器

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
    QJsonObject processStartCharge(const QJsonObject &req);
    QJsonObject processStopCharge(const QJsonObject &req);

    // 充电进度定时器控制(断线/结算后停止)
    void startProgressTimer(int orderId);
    void stopProgressTimer();

    qintptr m_descriptor;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_dbConnName;
    int m_userId = -1;      // 登录后绑定的用户

    QTimer *m_progressTimer = nullptr;   // 充电进度定时器(本线程事件循环驱动)
    int m_chargingOrderId = -1;          // 当前模拟中的订单
    int m_simMinutes = 0;                // 已模拟的充电分钟数
};

#endif // CLIENTHANDLER_H
