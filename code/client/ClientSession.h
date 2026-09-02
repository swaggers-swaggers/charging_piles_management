#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

#include <QString>

// 客户端当前登录的用户会话(单例)
// 登录成功后由 LoginDialog 填充(数据来自服务端), 各功能页面直接读取
struct ClientSession
{
    static ClientSession &instance();

    void reset();

    int userId = -1;
    QString phone;
    QString nickname;
    QString avatar;
    double balance = 0.0;

private:
    ClientSession() = default;
};

#endif // CLIENT_SESSION_H
