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

    // 手机号安全处理(隐私保护: 数据库不存明文手机号)
    // hashPhone: SHA-256(固定盐+手机号) → 16进制, 用于登录精确匹配与唯一存储
    // maskPhone: 脱敏显示 138****5678, 用于管理端展示与模糊搜索
    static QString hashPhone(const QString &phone);
    static QString maskPhone(const QString &phone);

    // 生成/重建近30天演示订单(销售业绩与大屏数据用)
    // 由 seedDemoOrders 在"今天尚无演示订单"时调用, 滚动到今天; 同一天内只生成一次
    void generateDemoData(QString *errMsg = nullptr);

private:
    DatabaseManager() = default;

    // 按优先级查找可用的数据库文件, 都不存在时返回工作目录下的 test.db
    QString resolveDatabaseFile() const;
    bool createTables(const QString &connName, QString *errMsg);
    void seedDefaultData();
    // 演示数据: 近30天固定订单(确定性伪随机, 表空时填充一次), 让营收/趋势/大屏有数据可看
    void seedDemoOrders();
    // 兼容旧库: 把 user.phone 明文迁移为哈希, 并回填 phone_masked 脱敏列
    void migratePhoneEncryption();

    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
