#include "ClientHandler.h"

#include "DatabaseManager.h"
#include "protocol.h"
#include "types.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_descriptor(socketDescriptor)
{
}

ClientHandler::~ClientHandler()
{
    if (!m_dbConnName.isEmpty()) {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
        if (db.isOpen())
            db.close();
        db = QSqlDatabase();   // 释放引用后再移除, 避免Qt告警
        QSqlDatabase::removeDatabase(m_dbConnName);
    }
}

void ClientHandler::start()
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(m_descriptor)) {
        qDebug() << "[ClientHandler] 套接字初始化失败";
        emit finished();
        return;
    }

    // 本线程私有的数据库连接
    m_dbConnName = QString("client-%1")
                       .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnName);
    db.setDatabaseName(DatabaseManager::instance().databasePath());
    if (db.open()) {
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA busy_timeout=3000");
    } else {
        qDebug() << "[ClientHandler] 数据库连接失败:" << db.lastError().text();
        // 连接保留, 具体业务请求会以数据库错误应答
    }

    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ClientHandler::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        // RemoteHostClosed 会紧随 disconnected, 其余错误记日志便于排查
        if (m_socket->error() != QAbstractSocket::RemoteHostClosedError)
            qDebug() << "[ClientHandler] 套接字错误:" << m_socket->errorString();
    });

    qDebug() << "[ClientHandler] 客户端接入:"
             << m_socket->peerAddress().toString() << m_socket->peerPort();
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // 防止异常数据撑爆内存
    if (m_buffer.size() > 1024 * 1024) {
        qDebug() << "[ClientHandler] 缓冲区超限, 断开连接";
        m_socket->disconnectFromHost();
        return;
    }

    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        processLine(line);
    }
}

void ClientHandler::onDisconnected()
{
    qDebug() << "[ClientHandler] 客户端断开";
    emit finished();
}

void ClientHandler::processLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty())
        return;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        sendError(0, "请求格式错误");
        return;
    }
    handleRequest(doc.object());
}

void ClientHandler::handleRequest(const QJsonObject &request)
{
    const int type = request.value("type").toInt();
    QJsonObject reply;

    switch (type) {
    case Protocol::ReqHeartbeat:
        reply = Protocol::makeReply(type, true);
        break;
    case Protocol::ReqUserLogin:
        reply = processUserLogin(request);
        break;
    default:
        sendError(type, "暂不支持的消息类型");
        return;
    }

    sendJson(reply);
}

QJsonObject ClientHandler::processUserLogin(const QJsonObject &req)
{
    const QString phone = req.value("phone").toString().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch())
        return Protocol::makeReply(Protocol::ReqUserLogin, false, "手机号格式不正确!");

    UserInfo info;
    bool isNewUser = false;
    QString errMsg;
    // 使用本线程的数据库连接
    if (!DatabaseManager::instance().loginOrRegisterUser(phone, &info, &isNewUser,
                                                         &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUserLogin, false, errMsg);

    m_userId = info.id;

    QJsonObject reply = Protocol::makeReply(Protocol::ReqUserLogin, true);
    reply.insert("userId", info.id);
    reply.insert("phone", info.phone);
    reply.insert("nickname", info.nickname);
    reply.insert("balance", info.balance);
    reply.insert("isNew", isNewUser);
    qDebug() << "[ClientHandler] 用户登录:" << phone
             << (isNewUser ? "(新注册)" : "") << "nickname=" << info.nickname;
    return reply;
}

void ClientHandler::sendJson(const QJsonObject &obj)
{
    if (!m_socket)
        return;
    m_socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void ClientHandler::sendError(int type, const QString &text)
{
    sendJson(Protocol::makeReply(type, false, text));
}
