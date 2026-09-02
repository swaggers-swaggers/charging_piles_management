#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QString>
#include <QTcpServer>

class QTcpSocket;

// 简易 HTTP 服务器: 为 Web 大数据可视化大屏提供静态文件服务
// (index.html / echarts.min.js / data.json 等), 支持 GET 请求与基本 MIME 类型
class HttpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit HttpServer(QObject *parent = nullptr);

    // 静态资源根目录(通常为 web/ 目录, 与 DataExporter::exportDir 一致)
    void setRoot(const QString &rootDir);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    QString m_root;
};

// 单个 HTTP 连接的处理器
class HttpConnection : public QObject
{
    Q_OBJECT

public:
    HttpConnection(qintptr socketDescriptor, const QString &root, QObject *parent = nullptr);

private slots:
    void onReady();

private:
    void closeWith(int statusCode, const QString &reason, const QByteArray &body,
                   const QByteArray &contentType);
    static QByteArray mimeFor(const QString &path);

    QTcpSocket *m_socket;
    QString m_root;
};

#endif // HTTPSERVER_H
