#include "HttpServer.h"

#include "DataExporter.h"

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

void HttpServer::setExporter(DataExporter *exporter)
{
    m_exporter = exporter;
}

void HttpServer::incomingConnection(qintptr socketDescriptor)
{
    new HttpConnection(socketDescriptor, m_root, m_exporter, this);
}

HttpConnection::HttpConnection(qintptr socketDescriptor, const QString &root,
                               DataExporter *exporter, QObject *parent)
    : QObject(parent)
    , m_root(root)
    , m_exporter(exporter)
{
    m_socket = new QTcpSocket(this);
    m_socket->setSocketDescriptor(socketDescriptor);
    connect(m_socket, &QTcpSocket::readyRead, this, &HttpConnection::onReady);
    connect(m_socket, &QTcpSocket::disconnected, m_socket, &QObject::deleteLater);
}

void HttpConnection::onReady()
{
    m_buffer += m_socket->readAll();
    // 等待完整请求头(GET 无 body, 请求头以 \r\n\r\n 结束)
    if (!m_buffer.contains("\r\n\r\n"))
        return;

    const QByteArray data = m_buffer;
    m_buffer.clear();

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
    // 剥离 query string(如 data.json?t=123456789), 否则无法命中 data.json 路由
    const int queryPos = rel.indexOf('?');
    if (queryPos >= 0)
        rel = rel.left(queryPos);
    if (rel.contains("..") || rel.contains('\\')) {
        closeWith(403, "Forbidden", "Forbidden", "text/plain");
        return;
    }
    if (rel == "/" || rel.isEmpty())
        rel = "index.html";
    else if (rel.startsWith('/'))
        rel = rel.mid(1);

    // /data.json: 优先实时从数据库聚合(不依赖落盘文件), 失败再回退读磁盘
    if (rel == "data.json" && m_exporter) {
        const QByteArray json = m_exporter->buildJson();
        if (!json.isEmpty()) {
            const QByteArray header =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Content-Length: " + QByteArray::number(json.size()) + "\r\n"
                "Cache-Control: no-cache\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n"
                "\r\n";
            m_socket->write(header);
            m_socket->write(json);
            m_socket->disconnectFromHost();
            return;
        }
    }

    const QString filePath = m_root + "/" + rel;
    QByteArray content;
    QString servePath = filePath;               // 用于 MIME 判断
    // 大屏静态页面(index.html/echarts.min.js)已打包进可执行文件资源(:/web/),
    // 优先从资源加载, 不再依赖服务端能否找到磁盘 web/ 目录; data.json 必须从磁盘读(动态导出)
    if (rel != "data.json") {
        QFile res(":/web/" + rel);
        if (res.open(QIODevice::ReadOnly)) {
            content = res.readAll();
            res.close();
            servePath = "web/" + rel;
        }
    }
    if (content.isEmpty()) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            // 返回带提示的页面, 避免一片空白让使用者误以为是"没数据"
            const QByteArray body =
                "<html><head><meta charset='utf-8'></head>"
                "<body style='font-family:sans-serif;padding:40px;line-height:1.8'>"
                "<h2>文件不存在: " + rel.toUtf8() + "</h2>"
                "<p>页面文件已打包进程序, 这里通常是 <b>data.json</b>(大屏数据) 未生成。</p>"
                "<p>排查:</p>"
                "<ol>"
                "<li>data.json 由服务端每 10 秒自动导出到当前 web 目录;</li>"
                "<li>若缺失说明订单表尚无数据: 管理端销售页点\"生成演示数据\", 或删除数据库后重启服务端;</li>"
                "<li>也可用环境变量指定导出目录: <b>CHARGING_WEB_DIR=/绝对路径/web</b> 后重启服务端。</li>"
                "</ol>"
                "</body></html>";
            closeWith(404, "Not Found", body, "text/html; charset=utf-8");
            return;
        }
        content = file.readAll();
        file.close();
        servePath = filePath;
    }

    const QByteArray type = mimeFor(servePath);
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
