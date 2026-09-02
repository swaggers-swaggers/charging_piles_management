#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QTcpServer>

// 面向用户客户端的 TCP 服务端
// 每个客户端连接分配一个独立工作线程(QThread), 连接的读写、业务处理、
// 数据库访问都在该线程内完成(多线程考核点, 见 ClientHandler)
class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

#endif // TCPSERVER_H
