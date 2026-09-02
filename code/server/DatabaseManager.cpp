#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

// 数据库文件查找优先级:
//   1. 环境变量 CHARGING_DB 指定的路径
//   2. 工作目录 / 可执行文件目录附近的 test.db、database/test.db
//   3. 都不存在时, 在工作目录新建 test.db (SQLite 会自动创建空文件)
// 在虚拟机里如需固定数据库位置, 可执行: export CHARGING_DB=/path/to/test.db
DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager mgr;
    return mgr;
}

bool DatabaseManager::init(QString *errMsg)
{
    m_dbPath = resolveDatabaseFile();

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        if (errMsg)
            *errMsg = QString("无法打开数据库 %1:\n%2").arg(m_dbPath, db.lastError().text());
        return false;
    }

    // WAL 模式允许管理界面(主线程)与网络线程的连接并发读写; busy_timeout 缓解写锁冲突
    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA busy_timeout=3000");

    if (!createTables(QString(), errMsg))
        return false;

    seedDefaultData();

    qDebug() << "[Database] 已连接:" << m_dbPath;
    return true;
}

QString DatabaseManager::databasePath() const
{
    return m_dbPath;
}

QString DatabaseManager::resolveDatabaseFile() const
{
    const QString envPath = qEnvironmentVariable("CHARGING_DB");
    if (!envPath.isEmpty())
        return envPath;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        "test.db",
        appDir + "/test.db",
        appDir + "/../test.db",
        appDir + "/../../test.db",
        appDir + "/../database/test.db",
        appDir + "/../../database/test.db",
        "database/test.db",
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }

    return "test.db";
}

bool DatabaseManager::createTables(const QString &connName, QString *errMsg)
{
    // 状态字段约定:
    //   user.status    0-正常 1-冻结
    //   pile.type      0-快充 1-慢充
    //   pile.status    0-闲置 1-在用 2-故障
    //   charge_order.status  0-充电中 1-已完成
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS admin ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " username TEXT UNIQUE NOT NULL,"
        " password TEXT NOT NULL,"
        " create_time TEXT DEFAULT (datetime('now','localtime')))",

        "CREATE TABLE IF NOT EXISTS user ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " phone TEXT UNIQUE NOT NULL,"
        " nickname TEXT DEFAULT '',"
        " avatar TEXT DEFAULT '',"
        " balance REAL DEFAULT 0,"
        " status INTEGER DEFAULT 0,"
        " register_time TEXT DEFAULT (datetime('now','localtime')))",

        "CREATE TABLE IF NOT EXISTS station ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " address TEXT DEFAULT '',"
        " longitude REAL DEFAULT 0,"
        " latitude REAL DEFAULT 0,"
        " price REAL DEFAULT 1.0,"
        " create_time TEXT DEFAULT (datetime('now','localtime')))",

        "CREATE TABLE IF NOT EXISTS pile ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " station_id INTEGER NOT NULL,"
        " code TEXT NOT NULL,"
        " type INTEGER DEFAULT 0,"
        " power REAL DEFAULT 0,"
        " status INTEGER DEFAULT 0,"
        " total_count INTEGER DEFAULT 0,"
        " total_duration INTEGER DEFAULT 0,"
        " FOREIGN KEY(station_id) REFERENCES station(id))",

        "CREATE TABLE IF NOT EXISTS charge_order ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " pile_id INTEGER NOT NULL,"
        " station_id INTEGER NOT NULL,"
        " start_time TEXT,"
        " end_time TEXT,"
        " energy REAL DEFAULT 0,"
        " amount REAL DEFAULT 0,"
        " status INTEGER DEFAULT 0,"
        " FOREIGN KEY(user_id) REFERENCES user(id),"
        " FOREIGN KEY(pile_id) REFERENCES pile(id),"
        " FOREIGN KEY(station_id) REFERENCES station(id))",

        "CREATE TABLE IF NOT EXISTS op_log ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " op_time TEXT DEFAULT (datetime('now','localtime')),"
        " op_user TEXT DEFAULT '',"
        " action TEXT DEFAULT '',"
        " detail TEXT DEFAULT '')",

        "CREATE INDEX IF NOT EXISTS idx_pile_station ON pile(station_id)",
        "CREATE INDEX IF NOT EXISTS idx_order_user ON charge_order(user_id)",
    };

    QSqlQuery query(QSqlDatabase::database(connName));
    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            if (errMsg)
                *errMsg = "初始化数据表失败: " + query.lastError().text();
            return false;
        }
    }
    return true;
}

void DatabaseManager::seedDefaultData()
{
    QSqlQuery query;

    // 默认管理员 admin / 123456 (项目说明书: 账号密码存储在数据库管理员表中)
    // 密码以 MD5 摘要存储; verifyAdmin 兼容历史明文记录并自动升级
    const QString md5Pwd = QString::fromLatin1(
        QCryptographicHash::hash("123456", QCryptographicHash::Md5).toHex());
    query.prepare("INSERT OR IGNORE INTO admin (username, password) VALUES (:u, :p)");
    query.bindValue(":u", "admin");
    query.bindValue(":p", md5Pwd);
    query.exec();

    // 固定种子数据: 充电站/电桩分布(仅表为空时写入, 多次运行完全一致)
    query.exec("SELECT COUNT(*) FROM station");
    if (query.next() && query.value(0).toInt() > 0)
        return;

    const struct StationSeed {
        QString name;
        QString address;
        double longitude;
        double latitude;
        double price;   // 元/度
        int pileCount;
    } seeds[] = {
        { "东软软件园充电站", "沈阳市浑南区创新路399号",  123.4572, 41.6562, 0.98, 6 },
        { "奥体中心充电站",   "沈阳市浑南区浑南中路16号", 123.4401, 41.7373, 1.20, 8 },
        { "桃仙机场充电站",   "沈阳市浑南区桃仙大街88号", 123.4830, 41.6398, 1.35, 4 },
    };

    int codeSeq = 1;
    for (const StationSeed &s : seeds) {
        query.prepare("INSERT INTO station (name, address, longitude, latitude, price) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(s.name);
        query.addBindValue(s.address);
        query.addBindValue(s.longitude);
        query.addBindValue(s.latitude);
        query.addBindValue(s.price);
        if (!query.exec())
            continue;
        const int stationId = query.lastInsertId().toInt();

        for (int i = 0; i < s.pileCount; ++i) {
            const int type = (i % 2 == 0) ? PileFast : PileSlow;
            const double power = (type == PileFast) ? 60.0 : 7.0;  // kW
            int status = PileIdle;
            if (codeSeq % 5 == 0) status = PileInUse;              // 示例: 部分在用
            if (codeSeq % 7 == 0) status = PileFault;              // 示例: 少量故障

            query.prepare("INSERT INTO pile (station_id, code, type, power, status) "
                          "VALUES (?, ?, ?, ?, ?)");
            query.addBindValue(stationId);
            query.addBindValue(QString("CP-%1").arg(codeSeq, 3, 10, QLatin1Char('0')));
            query.addBindValue(type);
            query.addBindValue(power);
            query.addBindValue(status);
            query.exec();
            ++codeSeq;
        }
    }
}

bool DatabaseManager::verifyAdmin(const QString &username, const QString &password,
                                  int *adminId, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("SELECT id, password FROM admin WHERE username = :u");
    query.bindValue(":u", username);

    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询管理员信息失败: " + query.lastError().text();
        return false;
    }

    if (!query.next()) {
        if (errMsg)
            *errMsg = "用户名或密码错误!";
        return false;
    }

    const int id = query.value(0).toInt();
    const QString stored = query.value(1).toString();
    const QString hashed = QString::fromLatin1(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5).toHex());

    if (stored != hashed) {
        // 兼容历史明文记录: 校验通过后自动升级为摘要存储
        if (stored != password) {
            if (errMsg)
                *errMsg = "用户名或密码错误!";
            return false;
        }
        QSqlQuery upgrade(QSqlDatabase::database(connName));
        upgrade.prepare("UPDATE admin SET password = :p WHERE id = :id");
        upgrade.bindValue(":p", hashed);
        upgrade.bindValue(":id", id);
        upgrade.exec();
    }

    if (adminId)
        *adminId = id;
    return true;
}

bool DatabaseManager::loginOrRegisterUser(const QString &phone, UserInfo *info,
                                          bool *isNewUser, QString *errMsg,
                                          const QString &connName)
{
    QSqlDatabase db = QSqlDatabase::database(connName);

    QSqlQuery query(db);
    query.prepare("SELECT id, nickname, balance, status FROM user WHERE phone = :p");
    query.bindValue(":p", phone);

    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询用户信息失败: " + query.lastError().text();
        return false;
    }

    if (query.next()) {
        if (query.value(3).toInt() == UserFrozen) {
            if (errMsg)
                *errMsg = "该账号已被冻结, 请联系管理员";
            return false;
        }
        if (info) {
            info->id = query.value(0).toInt();
            info->phone = phone;
            info->nickname = query.value(1).toString();
            info->balance = query.value(2).toDouble();
        }
        if (isNewUser)
            *isNewUser = false;
        return true;
    }

    // 手机号不存在, 自动注册新用户
    const QString nickname = "用户" + phone.right(4);
    query.prepare("INSERT INTO user (phone, nickname) VALUES (:p, :n)");
    query.bindValue(":p", phone);
    query.bindValue(":n", nickname);

    if (!query.exec()) {
        if (errMsg)
            *errMsg = "注册新用户失败: " + query.lastError().text();
        return false;
    }

    if (info) {
        info->id = query.lastInsertId().toInt();
        info->phone = phone;
        info->nickname = nickname;
        info->balance = 0.0;
    }
    if (isNewUser)
        *isNewUser = true;
    return true;
}
