#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QString>
#include <QTcpServer>

class QTcpSocket;
class DataExporter;

// 简易 HTTP 服务器: 为 Web 大数据可视化大屏提供静态文件服务
// (index.html / echarts.min.js 打包在资源, /data.json 实时从数据库聚合返回)
class HttpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit HttpServer(QObject *parent = nullptr);

    // 静态资源根目录(通常为 web/ 目录, 与 DataExporter::exportDir 一致)
    void setRoot(const QString &rootDir);

    // 绑定数据提供者: /data.json 请求时实时从数据库聚合返回, 不依赖落盘文件
    void setExporter(DataExporter *exporter);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    QString m_root;
    DataExporter *m_exporter = nullptr;
};

// 单个 HTTP 连接的处理器
class HttpConnection : public QObject
{
    Q_OBJECT

public:
    HttpConnection(qintptr socketDescriptor, const QString &root,
                   DataExporter *exporter, QObject *parent = nullptr);

private slots:
    void onReady();

private:
    void closeWith(int statusCode, const QString &reason, const QByteArray &body,
                   const QByteArray &contentType);
    static QByteArray mimeFor(const QString &path);

    QTcpSocket *m_socket;
    QByteArray m_buffer;   // 累积请求数据, 应对 TCP 分包
    QString m_root;
    DataExporter *m_exporter;
};

#endif // HTTPSERVER_H
