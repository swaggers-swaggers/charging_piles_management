#include "HttpServer.h"

#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QTcpSocket>

HttpServer::HttpServer(QObject *parent)
    : QTcpServer(parent)
{
}

void HttpServer::setRoot(const QString &rootDir)
{
    m_root = rootDir;
}

void HttpServer::incomingConnection(qintptr socketDescriptor)
{
    new HttpConnection(socketDescriptor, m_root, this);
}

HttpConnection::HttpConnection(qintptr socketDescriptor, const QString &root, QObject *parent)
    : QObject(parent)
    , m_root(root)
{
    m_socket = new QTcpSocket(this);
    m_socket->setSocketDescriptor(socketDescriptor);
    connect(m_socket, &QTcpSocket::readyRead, this, &HttpConnection::onReady);
    connect(m_socket, &QTcpSocket::disconnected, m_socket, &QObject::deleteLater);
}

void HttpConnection::onReady()
{
    const QByteArray data = m_socket->readAll();
    // 等待完整请求头
    if (!data.contains("\r\n\r\n"))
        return;

    // 解析请求行: "GET /path HTTP/1.1"
    const QList<QByteArray> lines = data.split('\n');
    if (lines.isEmpty()) {
        closeWith(400, "Bad Request", "Bad Request", "text/plain");
        return;
    }
    const QList<QByteArray> parts = lines[0].trimmed().split(' ');
    if (parts.size() < 2 || parts[0] != "GET") {
        closeWith(405, "Method Not Allowed", "Only GET supported", "text/plain");
        return;
    }

    // 规范化路径, 防目录穿越
    QString rel = QString::fromUtf8(parts[1]);
    if (rel.contains("..") || rel.contains('\\')) {
        closeWith(403, "Forbidden", "Forbidden", "text/plain");
        return;
    }
    if (rel == "/" || rel.isEmpty())
        rel = "index.html";
    else if (rel.startsWith('/'))
        rel = rel.mid(1);

    const QString filePath = m_root + "/" + rel;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        closeWith(404, "Not Found", "Not Found: " + rel.toUtf8(), "text/plain");
        return;
    }
    const QByteArray content = file.readAll();
    file.close();

    const QByteArray type = mimeFor(filePath);
    const QByteArray header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + type + "\r\n"
        "Content-Length: " + QByteArray::number(content.size()) + "\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n";
    m_socket->write(header);
    m_socket->write(content);
    m_socket->disconnectFromHost();
}

void HttpConnection::closeWith(int statusCode, const QString &reason,
                               const QByteArray &body, const QByteArray &contentType)
{
    const QByteArray header =
        "HTTP/1.1 " + QByteArray::number(statusCode) + " " + reason.toUtf8() + "\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n";
    m_socket->write(header);
    m_socket->write(body);
    m_socket->disconnectFromHost();
}

QByteArray HttpConnection::mimeFor(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "html" || ext == "htm")
        return "text/html; charset=utf-8";
    if (ext == "js")
        return "application/javascript; charset=utf-8";
    if (ext == "css")
        return "text/css; charset=utf-8";
    if (ext == "json")
        return "application/json; charset=utf-8";
    if (ext == "png")
        return "image/png";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "svg")
        return "image/svg+xml";
    if (ext == "ico")
        return "image/x-icon";
    return "application/octet-stream";
}
