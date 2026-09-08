#include "TcpClient.h"

#include "TcpClientWorker.h"
#include "protocol.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QMetaType>
#include <QThread>
#include <QTimer>

TcpClient &TcpClient::instance()
{
    static TcpClient s;
    return s;
}

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
{
    m_host = Protocol::serverHost();
    m_port = Protocol::serverPort();
    // 跨线程信号参数需要注册元类型
    qRegisterMetaType<QJsonObject>("QJsonObject");

    m_worker = new TcpClientWorker;   // 无 parent, 便于 moveToThread
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &TcpClient::doConnect, m_worker, &TcpClientWorker::connectToServer);
    connect(this, &TcpClient::sendRequest, m_worker, &TcpClientWorker::doRequest);
    connect(this, &TcpClient::cancelConnect, m_worker, &TcpClientWorker::cancelConnection);
    connect(m_worker, &TcpClientWorker::connectResult, this,
            [this](int generation, bool ok, const QString &error) {
        if (generation == m_generation) onConnectResult(ok, error);
    });
    connect(m_worker, &TcpClientWorker::requestDone, this, &TcpClient::onRequestDone);
    connect(m_worker, &TcpClientWorker::pushReceived, this, &TcpClient::onPushReceived);
    connect(m_worker, &TcpClientWorker::socketDisconnected, this, [this](int generation) {
        if (generation == m_generation) onSocketDisconnected();
    });

    m_thread->start();
}

TcpClient::~TcpClient()
{
    // 程序退出时停止网络线程, 避免 "QThread: Destroyed while thread is still running"
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
    }
    // finished -> deleteLater 已负责销毁 worker；再次 delete 会在退出时重复释放。
    m_worker = nullptr;
}

bool TcpClient::setEndpoint(const QString &host, int port)
{
    QHostAddress address(host.trimmed());
    if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isNull()
        || address == QHostAddress(QHostAddress::AnyIPv4)
        || address.isMulticast() || address == QHostAddress(QHostAddress::Broadcast)
        || port < 1 || port > 65535 || m_busy || m_connecting)
        return false;
    const QString normalized = address.toString();
    if (normalized == m_host && port == m_port)
        return true;
    emit cancelConnect(m_generation);
    ++m_generation;
    onSocketDisconnected();
    m_host = normalized;
    m_port = port;
    return true;
}

bool TcpClient::ensureConnected(int timeoutMs, QString *errMsg)
{
    if (m_connected)
        return true;

    if (m_connecting) {
        if (errMsg) *errMsg = "正在连接服务器，请稍候";
        return false;
    }
    m_connecting = true;
    QString errText;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject *ctx = new QObject(this);
    connect(this, &TcpClient::connectResult, ctx, [&](bool ok, const QString &e) {
        if (!ok)
            errText = e;
        loop.quit();
    });
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    emit doConnect(m_host, m_port, ++m_generation);
    timer.start(timeoutMs);
    loop.exec();
    // 超时/完成后再收到的连接结果不再处理, 防止悬空引用
    disconnect(this, &TcpClient::connectResult, ctx, nullptr);
    ctx->deleteLater();

    m_connecting = false;
    if (!m_connected) {
        emit cancelConnect(m_generation);
        ++m_generation; // 忽略超时后迟到的旧连接结果。
        if (errText.isEmpty()) {
            errText = QString("连接 %1:%2 超时：请检查服务端防火墙、本地网络权限，"
                              "以及 Wi-Fi 是否开启终端隔离")
                          .arg(m_host).arg(m_port);
        }
    }
    if (errMsg)
        *errMsg = errText;
    return m_connected;
}

QJsonObject TcpClient::request(int type, const QJsonObject &payload,
                               int timeoutMs, bool *ok)
{
    if (!m_connected) {
        QString connErr;
        if (!ensureConnected(3000, &connErr)) {
            if (ok)
                *ok = false;
            return Protocol::makeReply(type, false, "无法连接服务器(" +
                                       m_host + ":" +
                                       QString::number(m_port) + "): " + connErr);
        }
    }

    if (m_busy) {
        if (ok)
            *ok = false;
        return Protocol::makeReply(type, false, "上一次请求尚未完成, 请稍后再试");
    }
    m_busy = true;

    QJsonObject reply;
    bool done = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject *ctx = new QObject(this);
    connect(this, &TcpClient::replyArrived, ctx, [&](int t, const QJsonObject &r) {
        if (t == type) {
            reply = r;
            done = true;
            loop.quit();
        }
    });
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    emit sendRequest(type, payload, timeoutMs);
    timer.start(timeoutMs);
    loop.exec();
    disconnect(this, &TcpClient::replyArrived, ctx, nullptr);
    ctx->deleteLater();
    m_busy = false;

    if (ok)
        *ok = done && reply.value("ok").toBool();
    return done ? reply : Protocol::makeReply(type, false, "请求超时, 请检查服务端是否在线");
}

void TcpClient::onConnectResult(bool ok, const QString &error)
{
    m_connected = ok;
    emit connectResult(ok, error);
}

void TcpClient::onRequestDone(int type, const QJsonObject &reply)
{
    emit replyArrived(type, reply);
}

void TcpClient::onPushReceived(const QJsonObject &msg)
{
    emit pushReceived(msg);
}

void TcpClient::onSocketDisconnected()
{
    if (m_connected) {
        m_connected = false;
        emit connectionLost();
    }
}
