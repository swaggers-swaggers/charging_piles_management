#include "TcpClientWorker.h"

#include "protocol.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>

TcpClientWorker::TcpClientWorker(QObject *parent)
    : QObject(parent)
{
}

void TcpClientWorker::connectToServer()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        emit connectResult(true, QString());
        return;
    }

    if (!m_socket) {
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::readyRead,
                this, &TcpClientWorker::onReadyRead);
        connect(m_socket, &QTcpSocket::disconnected,
                this, &TcpClientWorker::socketDisconnected);
        connect(m_socket, &QTcpSocket::connected, this, [this] {
            emit connectResult(true, QString());
        });
        connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            if (m_socket->error() != QAbstractSocket::RemoteHostClosedError)
                emit connectResult(false, m_socket->errorString());
        });
    }

    m_socket->connectToHost(Protocol::serverHost(),
                            static_cast<quint16>(Protocol::serverPort()));
    // 连接结果通过 connected / errorOccurred 信号异步返回
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
