#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "types.h"

#include <QSqlDatabase>
#include <QString>

// 数据库连接与初始化(默认无名连接, 管理端页面共享; 每个客户端线程另开命名连接)
// - 启动时建表、写入默认管理员 admin/admin123 与 12 个演示充电站
// - SQLite WAL 模式, 避免管理端写入时大屏只读查询锁库
// - 数据库文件查找顺序: CHARGING_DB 环境变量 → 工作目录 test.db
//   → 可执行文件目录向上查找 → 都没有则在工作目录新建 test.db
class DatabaseManager
{
public:
    static DatabaseManager &instance();

    bool init(QString *errMsg = nullptr);
    QSqlDatabase &database() { return m_db; }
    QString databasePath() const { return m_dbPath; }

    // 用户免密登录: 按手机号哈希查用户, 不存在则自动注册
    // out->phone 返回脱敏手机号; isNew 标记是否本次新注册
    bool loginOrRegisterUser(const QString &rawPhone, UserInfo *out, bool *isNew,
                             QString *errMsg = nullptr,
                             const QString &connName = QString());

    // 管理员账号密码校验(加盐 SHA-256), 成功时回填 adminId
    bool verifyAdmin(const QString &username, const QString &password,
                     int *adminId, QString *errMsg = nullptr);

private:
    DatabaseManager() = default;
    QString resolveDatabaseFile() const;
    bool createTables(QString *errMsg = nullptr);
    void migratePhoneEncryption();   // 旧库手机号明文 → 哈希(只执行一次)
    void migrateV2Schema();          // v2: charge_order 扩展列平滑升级
    void migrateAdminSchema();       // 旧库 admin 表: password→password_hash, 补 salt 列
    void seedDefaultData();
    void seedDefaultFeeRules();      // v2: 生成默认峰谷平分时费率
    void seedDemoOrders();           // 生成近 30 天演示订单(空库时)

    QSqlDatabase m_db;
    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
