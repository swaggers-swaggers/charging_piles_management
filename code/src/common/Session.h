#ifndef SESSION_H
#define SESSION_H

#include <QString>

// 当前登录会话信息(单例)
// 登录成功后由 LoginDialog 填充, 各功能页面直接读取
struct Session
{
    static Session &instance();

    void reset();

    // 管理端
    bool isAdmin = false;
    int adminId = -1;
    QString adminName;

    // 用户端
    int userId = -1;
    QString phone;
    QString nickname;
    double balance = 0.0;

private:
    Session() = default;
};

#endif // SESSION_H
