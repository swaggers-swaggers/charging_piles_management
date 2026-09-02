#ifndef SERVER_SESSION_H
#define SERVER_SESSION_H

#include <QString>

// 服务端当前登录的管理员会话(单例)
struct ServerSession
{
    static ServerSession &instance();

    void reset();

    int adminId = -1;
    QString adminName;

private:
    ServerSession() = default;
};

#endif // SERVER_SESSION_H
