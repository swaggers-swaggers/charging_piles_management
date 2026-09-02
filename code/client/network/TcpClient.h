#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QJsonObject>
#include <QObject>

class QThread;
class TcpClientWorker;

// 客户端网络封装(单例)
// - 独立工作线程持有 QTcpSocket, UI 线程不直接操作套接字
// - request(): 同步风格的请求-应答调用(等待在事件循环中进行, 带超时), 界面层直接调用
// - pushReceived(): 服务端主动推送(如充电进度)转发给界面层
// - 同一时刻只允许一个未完成请求(串行化, 课程项目足够)
class TcpClient : public QObject
{
    Q_OBJECT

public:
    static TcpClient &instance();

    bool ensureConnected(int timeoutMs = 3000, QString *errMsg = nullptr);

    // 发送请求并等待应答; 失败时返回带 ok=false 的对象(网络错误写入 error 字段)
    QJsonObject request(int type, const QJsonObject &payload = QJsonObject(),
                        int timeoutMs = 5000, bool *ok = nullptr);

signals:
    // 供界面层订阅的服务端推送 / 连接状态
    void pushReceived(const QJsonObject &msg);
    void connectionLost();

    // ---- 内部信号: 通过队列连接调度工作线程 ----
    void doConnect();
    void sendRequest(int type, QJsonObject payload, int timeoutMs);
    void connectResult(bool ok, const QString &error);
    void replyArrived(int type, const QJsonObject &reply);

private:
    explicit TcpClient(QObject *parent = nullptr);

    void onConnectResult(bool ok, const QString &error);
    void onRequestDone(int type, const QJsonObject &reply);
    void onPushReceived(const QJsonObject &msg);
    void onSocketDisconnected();

    TcpClientWorker *m_worker = nullptr;
    QThread *m_thread = nullptr;
    bool m_connected = false;
    bool m_busy = false;
};

#endif // TCPCLIENT_H
