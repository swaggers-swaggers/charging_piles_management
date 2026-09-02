#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>

// 单个客户端连接的处理类, 运行在独立工作线程中:
//   套接字读写 / JSON 解析 / 业务处理(含数据库访问)全部在本线程完成
// 数据库: 每个线程创建独立的 QSqlDatabase 连接(QSqlDatabase 连接禁止跨线程共用),
//         连接名以线程 id 区分, 查询时通过 connName 参数传给 DatabaseManager
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

private:
    void processLine(const QByteArray &line);
    void handleRequest(const QJsonObject &request);
    void sendJson(const QJsonObject &obj);
    void sendError(int type, const QString &text);

    // 各请求的业务处理 (阶段 0 实现登录与心跳, 后续阶段逐步补齐其余消息)
    QJsonObject processUserLogin(const QJsonObject &req);

    qintptr m_descriptor;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_dbConnName;
    int m_userId = -1;     // 登录后绑定的用户
};

#endif // CLIENTHANDLER_H
