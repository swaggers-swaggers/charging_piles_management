#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "types.h"

#include <QString>

// 数据库管理(单例), 服务端专用
// 负责: 打开 SQLite 数据库 / 自动建表 / 默认数据 / 登录相关的查询
// 线程说明: QSqlDatabase 连接不能跨线程共用。主线程使用默认连接;
// 网络工作线程通过 connName 参数传入自己的连接名(见 network/ClientHandler)
class DatabaseManager
{
public:
    static DatabaseManager &instance();

    // 打开数据库并初始化表结构和默认数据, 失败时通过 errMsg 返回原因
    bool init(QString *errMsg = nullptr);

    QString databasePath() const;

    // 管理员登录校验(管理员表 admin, 默认账号 admin / 123456)
    bool verifyAdmin(const QString &username, const QString &password,
                     int *adminId = nullptr, QString *errMsg = nullptr,
                     const QString &connName = QString());

    // 用户手机号免密登录: 手机号存在则校验状态后返回用户信息,
    // 不存在则自动注册(默认昵称 "用户" + 手机号后4位)
    bool loginOrRegisterUser(const QString &phone, UserInfo *info = nullptr,
                             bool *isNewUser = nullptr, QString *errMsg = nullptr,
                             const QString &connName = QString());

private:
    DatabaseManager() = default;

    // 按优先级查找可用的数据库文件, 都不存在时返回工作目录下的 test.db
    QString resolveDatabaseFile() const;
    bool createTables(const QString &connName, QString *errMsg);
    void seedDefaultData();

    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
