#include "TcpServer.h"

#include "ClientHandler.h"

#include <QDebug>
#include <QThread>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
{
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    // 线程-连接模型: 每个客户端连接一个工作线程
    // handler 不能有 parent(moveToThread 要求), 线程结束时统一清理
    QThread *thread = new QThread;
    ClientHandler *handler = new ClientHandler(socketDescriptor);
    handler->moveToThread(thread);

    connect(thread, &QThread::started, handler, &ClientHandler::start);
    connect(handler, &ClientHandler::finished, thread, &QThread::quit);
    connect(handler, &ClientHandler::finished, handler, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();

    qDebug() << "[TcpServer] 新客户端接入, 工作线程已启动";
}
