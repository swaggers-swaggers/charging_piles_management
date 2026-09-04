#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QPair>
#include <QVector>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QString hashAdminPassword(const QString &password)
{
    static const QByteArray salt = "neusoft-admin-password-2026";
    return QString::fromLatin1(QCryptographicHash::hash(
        salt + password.toUtf8(), QCryptographicHash::Sha256).toHex());
}
}

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

    // 旧库升级必须先于演示数据匹配，避免明文用户被重复创建。
    migratePhoneEncryption();
    seedDefaultData();

    // 演示数据: 近30天固定订单(让销售业绩/大屏趋势有数据可看)
    seedDemoOrders();

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
        " phone_masked TEXT DEFAULT '',"
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
    // 密码以加盐 SHA-256 摘要存储; verifyAdmin 兼容历史 MD5/明文记录并自动升级
    const QString hashedPwd = hashAdminPassword("123456");
    query.prepare("INSERT OR IGNORE INTO admin (username, password) VALUES (:u, :p)");
    query.bindValue(":u", "admin");
    query.bindValue(":p", hashedPwd);
    query.exec();

    // 固定种子数据: 充电站/电桩分布(仅表为空时写入, 多次运行完全一致)
    query.exec("SELECT COUNT(*) FROM station");
    if (query.next() && query.value(0).toInt() > 0) {
        const int stationCount = query.value(0).toInt();
        if (stationCount != 12)
            qWarning() << "[Database] 现有站点数量为" << stationCount
                       << ", 与默认种子数量 12 不一致, 保留现有业务数据";
        return;
    }

    const struct StationSeed {
        QString name;
        QString address;
        double longitude;
        double latitude;
        double price;   // 元/度
        int pileCount;
    } seeds[] = {
        { "特来电五道口充电站",       "北京市海淀区成府路28号五道口购物中心停车场",    116.339065, 39.991117, 1.35, 8 },
        { "开迈斯国家体育馆充电站",   "北京市朝阳区天辰东路9号奥林匹克公园P3停车场",    116.387746, 39.997471, 1.60, 12 },
        { "中石化奥林匹克P2充电站",   "北京市朝阳区天辰西路水立方停车场",               116.387679, 39.993433, 1.20, 6 },
        { "昆仑网电望京南充电站",     "北京市朝阳区望京南加油站",                       116.482330, 40.013817, 1.25, 8 },
        { "国家电网大兴机场充电站",   "北京市大兴区天兴一街与航兴路交叉口南侧停车场",   116.420394, 39.529542, 1.50, 10 },
        { "小桔充电望京文化产业园站", "北京市朝阳区望京西路48-6号",                     116.479208, 39.994492, 1.10, 6 },
        { "普天望京凯德Mall充电站",   "北京市朝阳区广顺北大街33号凯德Mall停车场",       116.468897, 39.992102, 1.15, 4 },
        { "国家电网北京坊充电站",     "北京市西城区大栅栏煤市街北京坊B3停车场",         116.396702, 39.898266, 1.30, 6 },
        { "昆仑网电工体西门充电站",   "北京市东城区新中街东直门城市生态岛旁",           116.439840, 39.931522, 1.45, 8 },
        { "比亚迪通州科创充电站",     "北京市通州区科创东五街1号",                       116.551189, 39.813862, 1.05, 10 },
        { "小桔充电亚林西充电站",     "北京市丰台区南苑亚林西",                          116.350235, 39.854170, 0.95, 6 },
        { "高陆通成铭大厦充电站",     "北京市西城区西直门南大街2号成铭大厦B4停车场",    116.350637, 39.938143, 1.20, 4 },
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

// 演示用户(手机号仅存哈希+脱敏); 第一个用户作为"每日演示订单已生成"的哨兵
static const struct DemoUserSeed {
    const char *phone;
    const char *nickname;
    double balance;
    int regDaysAgo;   // 注册时间距今天数
} kDemoUsers[] = {
    { "13800000001", "演示用户A", 500.0, 90 },
    { "13911112222", "演示用户B", 260.0, 75 },
    { "13733334444", "演示用户C", 320.0, 60 },
    { "13655556666", "演示用户D", 150.0, 45 },
    { "13577778888", "演示用户E", 88.5, 30 },
    { "15899990001", "演示用户F", 410.0, 80 },
    { "15911112222", "演示用户G", 60.0, 20 },
    { "18633334444", "演示用户H", 200.0, 55 },
    { "18855556666", "演示用户I", 700.0, 100 },
    { "18577778888", "演示用户J", 35.0, 10 },
};

void DatabaseManager::seedDemoOrders()
{
    // 滚动 + 持久化: 演示数据始终覆盖到今天(今日/本月/近7日都有数据可看),
    // 但同一天内只生成一次(以第一个演示用户"今天是否有订单"为哨兵), 结果稳定可复现。
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM charge_order WHERE user_id ="
                  " (SELECT id FROM user WHERE phone = :p)"
                  " AND date(start_time) = date('now','localtime')");
    query.bindValue(":p", hashPhone(QString::fromUtf8(kDemoUsers[0].phone)));
    if (query.exec() && query.next() && query.value(0).toInt() > 0)
        return;

    QString err;
    generateDemoData(&err);
    if (err.isEmpty())
        qDebug() << "[Database] 已生成近30天北京演示订单数据";
    else
        qWarning() << "[Database] 生成演示数据失败:" << err;
}

void DatabaseManager::generateDemoData(QString *errMsg)
{
    QSqlQuery query;

    // 1. 确保演示用户存在(手机号仅存哈希 + 脱敏, 不落明文)
    QVector<int> userIds;
    for (const DemoUserSeed &u : kDemoUsers) {
        const QString phone = QString::fromUtf8(u.phone);
        const QString ph = hashPhone(phone);
        int uid = -1;
        query.prepare("SELECT id FROM user WHERE phone = :p");
        query.bindValue(":p", ph);
        if (query.exec() && query.next()) {
            uid = query.value(0).toInt();
        } else {
            query.prepare("INSERT INTO user (phone, phone_masked, nickname, balance, register_time) "
                          "VALUES (:p, :m, :n, :b, :r)");
            query.bindValue(":p", ph);
            query.bindValue(":m", maskPhone(phone));
            query.bindValue(":n", QString::fromUtf8(u.nickname));
            query.bindValue(":b", u.balance);
            query.bindValue(":r", QDateTime::currentDateTime().addDays(-u.regDaysAgo)
                            .toString("yyyy-MM-dd hh:mm:ss"));
            if (query.exec()) {
                uid = query.lastInsertId().toInt();
            } else {
                if (errMsg)
                    *errMsg = "创建演示用户失败: " + query.lastError().text();
                return;
            }
        }
        if (uid > 0)
            userIds.append(uid);
    }
    if (userIds.isEmpty()) {
        if (errMsg)
            *errMsg = "无法创建演示用户";
        return;
    }

    // 2. 清理演示用户的旧演示订单(避免重复运行累积)
    for (int uid : userIds) {
        query.prepare("DELETE FROM charge_order WHERE user_id = ?");
        query.addBindValue(uid);
        query.exec();
    }

    // 3. 充电站价格 + 电桩(含类型/功率/所属站)
    QMap<int, double> stationPrice;
    query.exec("SELECT id, price FROM station");
    while (query.next())
        stationPrice.insert(query.value(0).toInt(), query.value(1).toDouble());

    struct PileRow { int id; int stationId; int type; double power; };
    QVector<PileRow> piles;
    query.exec("SELECT id, station_id, type, power FROM pile");
    while (query.next())
        piles.append({ query.value(0).toInt(), query.value(1).toInt(),
                       query.value(2).toInt(), query.value(3).toDouble() });
    if (piles.isEmpty() || stationPrice.isEmpty()) {
        if (errMsg)
            *errMsg = "缺少充电站/电桩数据";
        return;
    }

    // 4. 确定性伪随机: 固定种子, 多次运行生成结果完全一致("固定下来")
    quint32 seed = 20260904u;
    auto rnd = [&seed]() -> quint32 {
        seed = seed * 1103515245u + 12345u;
        return (seed >> 16) & 0x7fff;
    };

    // 24h 充电需求权重(接近真实: 深夜低谷, 早晚通勤双高峰, 晚高峰最高)
    const int hourWeight[24] = {
        1, 1, 1, 1, 2, 4,     // 0-5  深夜低谷
        8, 12, 15, 12, 10, 10, // 6-11 早高峰(8-9)
        9, 9, 10, 11, 13, 16,  // 12-17 午间平稳
        18, 18, 15, 11, 7, 3   // 18-23 晚高峰(18-19)
    };
    int weightSum = 0;
    for (int w : hourWeight) weightSum += w;

    // 按权重抽取 0~23 点(真实分布)
    auto pickHour = [&]() -> int {
        int r = static_cast<int>(rnd() % weightSum);
        for (int h = 0; h < 24; ++h) {
            if (r < hourWeight[h]) return h;
            r -= hourWeight[h];
        }
        return 18;
    };

    const QDate today = QDate::currentDate();
    for (int day = 29; day >= 0; --day) {
        const QDate d = today.addDays(-day);
        const bool weekend = (d.dayOfWeek() == 6 || d.dayOfWeek() == 7);
        // 每天订单数: 工作日 18~39, 周末 22~45(出行多, 单量略高)
        const int nOrders = weekend ? 22 + static_cast<int>(rnd() % 24)
                                    : 18 + static_cast<int>(rnd() % 22);
        for (int i = 0; i < nOrders; ++i) {
            // 随机用户(演示用户均匀充电)
            const int ui = static_cast<int>(rnd() % userIds.size());
            // 随机桩
            const PileRow &pl = piles[static_cast<int>(rnd() % piles.size())];
            const double price = stationPrice.value(pl.stationId, 1.0);
            const int hour = pickHour();
            const int minute = static_cast<int>(rnd() % 60);

            // 按桩类型生成接近真实的时长/电量
            double energy = 0.0, duration = 0.0;
            if (pl.type == PileFast) {               // 快充: 15~55 kWh, 0.4~1.5h
                energy = 15.0 + static_cast<double>(rnd() % 41);
                duration = 0.4 + static_cast<double>(rnd() % 12) / 10.0;
            } else {                                 // 慢充: 10~40 kWh, 2.0~8.0h
                energy = 10.0 + static_cast<double>(rnd() % 31);
                duration = 2.0 + static_cast<double>(rnd() % 61) / 10.0;
            }
            const double amount = qRound(energy * price * 100.0) / 100.0;

            const QString startStr = QString("%1 %2:%3:00")
                .arg(d.toString("yyyy-MM-dd"))
                .arg(hour, 2, 10, QLatin1Char('0'))
                .arg(minute, 2, 10, QLatin1Char('0'));
            const QDateTime endDt =
                QDateTime::fromString(startStr, "yyyy-MM-dd hh:mm:ss")
                    .addSecs(qint64(duration * 3600));
            const QString endStr = endDt.toString("yyyy-MM-dd hh:mm:ss");

            query.prepare("INSERT INTO charge_order "
                          "(user_id, pile_id, station_id, start_time, end_time, energy, amount, status) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, 1)");
            query.addBindValue(userIds[ui]);
            query.addBindValue(pl.id);
            query.addBindValue(pl.stationId);
            query.addBindValue(startStr);
            query.addBindValue(endStr);
            query.addBindValue(energy);
            query.addBindValue(amount);
            query.exec();
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
    const QString hashed = hashAdminPassword(password);
    const QString legacyMd5 = QString::fromLatin1(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5).toHex());

    if (stored != hashed) {
        // 兼容历史 MD5/明文记录: 校验通过后自动升级为加盐 SHA-256
        if (stored != legacyMd5 && stored != password) {
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

    // 数据库只存哈希(不可逆) + 脱敏号, 不落明文手机号
    const QString phoneHash = hashPhone(phone);
    const QString masked = maskPhone(phone);

    QSqlQuery query(db);
    query.prepare("SELECT id, nickname, balance, status FROM user WHERE phone = :p");
    query.bindValue(":p", phoneHash);

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
            info->phone = masked;
            info->nickname = query.value(1).toString();
            info->balance = query.value(2).toDouble();
        }
        if (isNewUser)
            *isNewUser = false;
        return true;
    }

    // 手机号不存在, 自动注册新用户
    const QString nickname = "用户" + phone.right(4);
    query.prepare("INSERT INTO user (phone, phone_masked, nickname) VALUES (:p, :m, :n)");
    query.bindValue(":p", phoneHash);
    query.bindValue(":m", masked);
    query.bindValue(":n", nickname);

    if (!query.exec()) {
        if (errMsg)
            *errMsg = "注册新用户失败: " + query.lastError().text();
        return false;
    }

    if (info) {
        info->id = query.lastInsertId().toInt();
        info->phone = masked;
        info->nickname = nickname;
        info->balance = 0.0;
    }
    if (isNewUser)
        *isNewUser = true;
    return true;
}

QString DatabaseManager::hashPhone(const QString &phone)
{
    // 固定应用级盐 + SHA-256: 同一手机号哈希稳定(可精确匹配), 但不可逆还原明文
    static const QByteArray kSalt = "neusoft-charging-platform-2026";
    return QString::fromLatin1(
        QCryptographicHash::hash(kSalt + phone.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString DatabaseManager::maskPhone(const QString &phone)
{
    // 脱敏: 保留前3后4, 中间变星号; 如 13812345678 → 138****5678
    if (phone.size() <= 7)
        return phone.left(3) + "****";
    return phone.left(3) + "****" + phone.right(4);
}

void DatabaseManager::migratePhoneEncryption()
{
    QSqlDatabase db = QSqlDatabase::database();

    // 旧库升级(关键): 老版本 user 表没有 phone_masked 列, CREATE TABLE IF NOT EXISTS
    // 不会给已存在的表加列, 必须先 ALTER 补列, 否则后续 INSERT/SELECT 都会失败
    {
        QSqlQuery colCheck(db);
        if (!colCheck.exec("SELECT phone_masked FROM user LIMIT 1")) {
            QSqlQuery alter(db);
            if (alter.exec("ALTER TABLE user ADD COLUMN phone_masked TEXT DEFAULT ''"))
                qDebug() << "[Database] 已为旧库 user 表补充 phone_masked 列";
            else
                qWarning() << "[Database] 补充 phone_masked 列失败:" << alter.lastError().text();
        }
    }

    QSqlQuery check(db);
    if (!check.exec("SELECT phone, id FROM user"))
        return;

    QList<QPair<int, QString>> migrate;   // (id, 明文手机号)
    while (check.next()) {
        const QString phone = check.value(0).toString();
        // 已加密的哈希为 64 位 hex, 明文(手机号)一般 ≤ 11 位, 据此区分
        if (phone.length() == 64)
            continue;
        migrate.append({ check.value(1).toInt(), phone });
    }
    if (migrate.isEmpty())
        return;

    for (const auto &row : migrate) {
        const QString masked = maskPhone(row.second);
        QSqlQuery up(db);
        up.prepare("UPDATE user SET phone = :h, phone_masked = :m WHERE id = :id");
        up.bindValue(":h", hashPhone(row.second));
        up.bindValue(":m", masked);
        up.bindValue(":id", row.first);
        up.exec();
    }
    qDebug() << "[Database] 已完成" << migrate.size() << "条手机号加密迁移";
}
