#include "TcpClientWorker.h"

#include "protocol.h"

#include <QDebug>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonParseError>

TcpClientWorker::TcpClientWorker(QObject *parent)
    : QObject(parent)
{
}

void TcpClientWorker::cancelConnection(int generation)
{
    if (generation != m_generation)
        return;
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();
}

void TcpClientWorker::connectToServer(const QString &host, int port, int generation)
{
    cancelConnection(m_generation);
    m_generation = generation;
    m_socket = new QTcpSocket(this);
    // 局域网业务连接直连，地图请求仍可使用系统代理。
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClientWorker::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this, generation] {
        m_buffer.clear();
        emit socketDisconnected(generation);
    });
    connect(m_socket, &QTcpSocket::connected, this, [this, generation] {
        emit connectResult(generation, true, QString());
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this, generation](QAbstractSocket::SocketError) {
        emit connectResult(generation, false, m_socket->errorString());
    });
    m_socket->connectToHost(host, static_cast<quint16>(port));
}

void TcpClientWorker::doRequest(int type, QJsonObject payload, int timeoutMs)
{
    Q_UNUSED(timeoutMs);   // 超时等待由 TcpClient 侧的定时器控制

    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit requestDone(type, Protocol::makeReply(type, false,
                                                   "未连接服务器, 请先启动服务端 ChargingServer"));
        return;
    }

    payload.insert("type", type);
    m_socket->write(QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n');
}

void TcpClientWorker::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        if (line.trimmed().isEmpty())
            continue;

        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        const int type = obj.value("type").toInt();
        if (type >= 100)
            emit pushReceived(obj);       // 服务端主动推送(如充电进度)
        else
            emit requestDone(type, obj);  // 请求的应答
    }
}
