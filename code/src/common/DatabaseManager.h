#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>

// 用户表记录
struct UserInfo
{
    int id = -1;
    QString phone;
    QString nickname;
    double balance = 0.0;
};

// 数据库管理(单例)
// 负责: 打开 SQLite 数据库 / 自动建表 / 默认数据 / 登录相关的查询
// 后续各功能模块的增删改查建议按模块补充 Dao 类(如 StationDao), 或继续扩充本类
class DatabaseManager
{
public:
    static DatabaseManager &instance();

    // 打开数据库并初始化表结构和默认数据, 失败时通过 errMsg 返回原因
    bool init(QString *errMsg = nullptr);

    bool isOpen() const;
    QString databasePath() const;

    // 管理员登录校验(管理员表 admin, 默认账号 admin / 123456)
    bool verifyAdmin(const QString &username, const QString &password,
                     int *adminId = nullptr, QString *errMsg = nullptr);

    // 用户手机号免密登录: 手机号存在则校验状态后返回用户信息,
    // 不存在则自动注册(默认昵称 "用户" + 手机号后4位)
    bool loginOrRegisterUser(const QString &phone, UserInfo *info = nullptr,
                             bool *isNewUser = nullptr, QString *errMsg = nullptr);

private:
    DatabaseManager() = default;

    // 按优先级查找可用的数据库文件, 都不存在时返回工作目录下的 test.db
    QString resolveDatabaseFile() const;
    bool createTables(QString *errMsg);
    void seedDefaultData();

    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
