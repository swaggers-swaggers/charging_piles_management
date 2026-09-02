#ifndef TCPCLIENTWORKER_H
#define TCPCLIENTWORKER_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>

// 客户端网络工作对象, 运行在独立线程中(由 TcpClient 单例创建并调度)
// 职责: 建立连接 / 发送请求 / 收取应答与服务端推送
class TcpClientWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpClientWorker(QObject *parent = nullptr);

public slots:
    void connectToServer();                                   // 建立到服务端的连接
    void doRequest(int type, QJsonObject payload, int timeoutMs);

signals:
    void connectResult(bool ok, const QString &error);
    void requestDone(int type, const QJsonObject &reply);
    void pushReceived(const QJsonObject &msg);
    void socketDisconnected();

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
};

#endif // TCPCLIENTWORKER_H
