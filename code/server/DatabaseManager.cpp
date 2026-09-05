#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QCryptographicHash>

namespace {
// 手机号 SHA-256 哈希(登录按哈希匹配, 避免明文存储)
// 固定应用级盐 + SHA-256: 同一手机号哈希稳定(可精确匹配), 但不可逆还原明文
// 注意: 盐必须与历史版本一致, 否则旧库用户会因哈希不匹配被重复注册
QString hashPhone(const QString &phone)
{
    static const QByteArray kSalt = "neusoft-charging-platform-2026";
    return QString::fromLatin1(
        QCryptographicHash::hash(kSalt + phone.toUtf8(), QCryptographicHash::Sha256).toHex());
}

// 手机号脱敏: 保留前 3 位与后 4 位, 中间用 **** 代替
QString maskPhone(const QString &phone)
{
    if (phone.size() < 7)
        return phone;
    return phone.left(3) + "****" + phone.right(4);
}
} // namespace

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

QString DatabaseManager::resolveDatabaseFile() const
{
    // 1. 环境变量显式指定
    const QString env = QString::fromLocal8Bit(qgetenv("CHARGING_DB"));
    if (!env.isEmpty())
        return QDir::fromNativeSeparators(env);

    // 2. 工作目录下的 test.db
    const QString cwd = QDir::current().filePath("test.db");
    if (QFileInfo::exists(cwd))
        return cwd;

    // 3. 从可执行文件目录向上最多 3 级查找 test.db / database/test.db
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 3; ++i) {
        const QString direct = dir.filePath("test.db");
        if (QFileInfo::exists(direct))
            return direct;
        const QString under = dir.filePath("database/test.db");
        if (QFileInfo::exists(under))
            return under;
        if (!dir.cdUp())
            break;
    }

    // 4. 都没有: 在工作目录新建
    return cwd;
}

bool DatabaseManager::init(QString *errMsg)
{
    m_dbPath = resolveDatabaseFile();
    m_db = QSqlDatabase::addDatabase("QSQLITE");   // 默认无名连接
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        if (errMsg)
            *errMsg = QStringLiteral("数据库打开失败(%1): %2")
                          .arg(m_dbPath, m_db.lastError().text());
        return false;
    }
    // WAL 模式: 管理端写订单/统计时, 大屏只读连接不会锁库
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA busy_timeout=3000");
    pragma.exec("PRAGMA foreign_keys=ON");

    if (!createTables(errMsg))
        return false;

    // 兼容旧库: 手机号由明文升级为哈希存储(只执行一次)
    migratePhoneEncryption();
    // v2: 订单扩展列(旧库平滑升级, 新库建表时已包含)
    migrateV2Schema();
    migrateAdminSchema();

    seedDefaultData();
    seedDefaultFeeRules();
    seedDemoOrders();

    // 修复历史数据: 哈希算法变更导致同一手机号被重复注册, 启动时自动合并
    deduplicateUsers();
    return true;
}

bool DatabaseManager::loginOrRegisterUser(const QString &rawPhone, UserInfo *out,
                                          bool *isNew, QString *errMsg,
                                          const QString &connName)
{
    QSqlDatabase db = connName.isEmpty() ? m_db : QSqlDatabase::database(connName);
    const QString hashed = hashPhone(rawPhone);

    QSqlQuery q(db);
    q.prepare("SELECT id, phone_masked, nickname, avatar, balance, status, register_time"
              " FROM user WHERE phone=?");
    q.addBindValue(hashed);
    if (!q.exec()) {
        if (errMsg) *errMsg = "登录查询失败: " + q.lastError().text();
        return false;
    }
    if (q.next()) {
        if (isNew) *isNew = false;
        if (out) {
            out->id = q.value(0).toInt();
            out->phone = q.value(1).toString();
            out->nickname = q.value(2).toString();
            out->avatar = q.value(3).toString();
            out->balance = q.value(4).toDouble();
            out->status = q.value(5).toInt();
            out->registerTime = q.value(6).toString();
        }
        return true;
    }

    // 新用户自动注册
    const QString masked = maskPhone(rawPhone);
    const QString nick = QStringLiteral("充电用户%1").arg(rawPhone.right(4));
    QSqlQuery ins(db);
    ins.prepare("INSERT INTO user(phone, phone_masked, nickname, balance) VALUES(?,?,?,0)");
    ins.addBindValue(hashed);
    ins.addBindValue(masked);
    ins.addBindValue(nick);
    if (!ins.exec()) {
        if (errMsg) *errMsg = "注册失败: " + ins.lastError().text();
        return false;
    }
    if (isNew) *isNew = true;
    if (out) {
        out->id = ins.lastInsertId().toInt();
        out->phone = masked;
        out->nickname = nick;
        out->balance = 0.0;
        out->status = UserNormal;
    }
    return true;
}

bool DatabaseManager::verifyAdmin(const QString &username, const QString &password,
                                  int *adminId, QString *errMsg)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id, password_hash, salt FROM admin WHERE username=?");
    q.addBindValue(username);
    if (!q.exec() || !q.next()) {
        if (errMsg)
            *errMsg = QStringLiteral("账号不存在");
        return false;
    }
    const int id = q.value(0).toInt();
    const QString stored = q.value(1).toString();
    const QString salt = q.value(2).toString();
    const QString calc = QString::fromLatin1(
        QCryptographicHash::hash((salt + password).toUtf8(),
                                 QCryptographicHash::Sha256).toHex());
    if (calc != stored) {
        if (errMsg)
            *errMsg = QStringLiteral("密码错误");
        return false;
    }
    if (adminId)
        *adminId = id;
    return true;
}

bool DatabaseManager::createTables(QString *errMsg)
{
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS admin ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " username TEXT UNIQUE NOT NULL,"
        " password_hash TEXT NOT NULL,"
        " salt TEXT DEFAULT '',"
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
        " power REAL DEFAULT 60,"
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
        " freeze_amount REAL DEFAULT 0,"
        " target_type INTEGER DEFAULT 0,"
        " target_value REAL DEFAULT 0,"
        " price_snapshot REAL DEFAULT 0,"
        " finish_type INTEGER DEFAULT 0,"
        " cancel_reason TEXT DEFAULT '',"
        " refund_amount REAL DEFAULT 0,"
        " sim_minutes INTEGER DEFAULT 0,"
        " FOREIGN KEY(user_id) REFERENCES user(id),"
        " FOREIGN KEY(pile_id) REFERENCES pile(id),"
        " FOREIGN KEY(station_id) REFERENCES station(id))",

        "CREATE TABLE IF NOT EXISTS price_rule ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " station_id INTEGER NOT NULL,"
        " period INTEGER DEFAULT 1,"
        " start_time TEXT DEFAULT '00:00',"
        " end_time TEXT DEFAULT '24:00',"
        " price REAL DEFAULT 1.0,"
        " service_fee REAL DEFAULT 0.0,"
        " FOREIGN KEY(station_id) REFERENCES station(id))",

        "CREATE TABLE IF NOT EXISTS charge_reservation ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " pile_id INTEGER NOT NULL,"
        " station_id INTEGER NOT NULL,"
        " type INTEGER DEFAULT 0,"
        " create_time TEXT DEFAULT (datetime('now','localtime')),"
        " assign_time TEXT,"
        " expire_time TEXT,"
        " reserve_date TEXT,"
        " reserve_start TEXT,"
        " reserve_end TEXT,"
        " status INTEGER DEFAULT 0,"
        " remind_sent INTEGER DEFAULT 0,"
        " FOREIGN KEY(user_id) REFERENCES user(id),"
        " FOREIGN KEY(pile_id) REFERENCES pile(id),"
        " FOREIGN KEY(station_id) REFERENCES station(id))",

        "CREATE TABLE IF NOT EXISTS recharge_log ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " amount REAL NOT NULL,"
        " balance_after REAL NOT NULL,"
        " create_time TEXT DEFAULT (datetime('now','localtime')),"
        " FOREIGN KEY(user_id) REFERENCES user(id))",

        "CREATE TABLE IF NOT EXISTS op_log ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " op_time TEXT DEFAULT (datetime('now','localtime')),"
        " op_user TEXT DEFAULT '',"
        " action TEXT DEFAULT '',"
        " detail TEXT DEFAULT '')",

        "CREATE INDEX IF NOT EXISTS idx_pile_station ON pile(station_id)",
        "CREATE INDEX IF NOT EXISTS idx_order_user ON charge_order(user_id)",
        "CREATE INDEX IF NOT EXISTS idx_order_status ON charge_order(status)",
        "CREATE INDEX IF NOT EXISTS idx_order_pile ON charge_order(pile_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_reservation_pile ON charge_reservation(pile_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_reservation_user ON charge_reservation(user_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_price_rule_station ON price_rule(station_id)",
        "CREATE INDEX IF NOT EXISTS idx_recharge_log_user ON recharge_log(user_id)",
    };

    for (const QString &sql : statements) {
        QSqlQuery q(m_db);
        if (!q.exec(sql)) {
            if (errMsg)
                *errMsg = QStringLiteral("建表失败: %1 | SQL: %2")
                              .arg(q.lastError().text(), sql);
            return false;
        }
    }
    return true;
}

void DatabaseManager::migratePhoneEncryption()
{
    // 检测是否仍是明文手机号: 能读出 11 位数字即旧库
    QSqlQuery probe(m_db);
    if (!probe.exec("SELECT id, phone FROM user LIMIT 1"))
        return;
    if (!probe.next())
        return;
    const QString sample = probe.value(1).toString();
    if (sample.size() != 11)
        return;   // 已是哈希(64位hex), 无需迁移

    QSqlQuery add(m_db);
    add.exec("ALTER TABLE user ADD COLUMN phone_masked TEXT DEFAULT ''");

    QSqlQuery all(m_db);
    all.exec("SELECT id, phone FROM user");
    struct Row { int id; QString phone; };
    QList<Row> rows;
    while (all.next())
        rows.append({ all.value(0).toInt(), all.value(1).toString() });
    for (const Row &r : rows) {
        QSqlQuery up(m_db);
        up.prepare("UPDATE user SET phone=?, phone_masked=? WHERE id=?");
        up.addBindValue(hashPhone(r.phone));
        up.addBindValue(maskPhone(r.phone));
        up.addBindValue(r.id);
        up.exec();
    }
}

void DatabaseManager::migrateV2Schema()
{
    // 收集 charge_order 现有列, 缺什么补什么(SQLite 不支持 ADD COLUMN IF NOT EXISTS)
    QSet<QString> existing;
    QSqlQuery ti(m_db);
    if (ti.exec("PRAGMA table_info(charge_order)")) {
        while (ti.next())
            existing.insert(ti.value(1).toString().toLower());
    }
    struct Col { QString name; QString ddl; };
    const QList<Col> need = {
        { "freeze_amount",  "REAL DEFAULT 0" },
        { "target_type",    "INTEGER DEFAULT 0" },
        { "target_value",   "REAL DEFAULT 0" },
        { "price_snapshot", "REAL DEFAULT 0" },
        { "finish_type",    "INTEGER DEFAULT 0" },
        { "cancel_reason",  "TEXT DEFAULT ''" },
        { "refund_amount",  "REAL DEFAULT 0" },
        { "sim_minutes",    "INTEGER DEFAULT 0" },
    };
    for (const Col &c : need) {
        if (existing.contains(c.name))
            continue;
        QSqlQuery add(m_db);
        add.exec(QString("ALTER TABLE charge_order ADD COLUMN %1 %2").arg(c.name, c.ddl));
    }
}

void DatabaseManager::migrateAdminSchema()
{
    // 旧库 admin 表: 列名为 password, 无 salt 列; 统一为 password_hash + salt
    QSet<QString> cols;
    QSqlQuery ti(m_db);
    if (ti.exec("PRAGMA table_info(admin)")) {
        while (ti.next())
            cols.insert(ti.value(1).toString().toLower());
    }
    if (cols.contains("password") && !cols.contains("password_hash")) {
        QSqlQuery q(m_db);
        q.exec("ALTER TABLE admin RENAME COLUMN password TO password_hash");
        cols.remove("password");
        cols.insert("password_hash");
    }
    if (!cols.contains("salt")) {
        QSqlQuery q(m_db);
        q.exec("ALTER TABLE admin ADD COLUMN salt TEXT DEFAULT ''");
        // 旧记录全部使用固定应用级盐, 与 hashAdminPassword 旧逻辑一致
        q.exec("UPDATE admin SET salt = 'neusoft-admin-password-2026' WHERE salt = ''");
    }
}

void DatabaseManager::deduplicateUsers()
{
    // 因 hashPhone 算法历史变更, 同一手机号可能被注册为多条 user 记录
    // (phone 哈希不同, 但 phone_masked 相同). 按 phone_masked 分组, 保留最早注册的,
    // 把其余用户的订单/充值/预约/余额合并到保留用户后删除.
    QSqlQuery q(m_db);
    q.exec("SELECT phone_masked, MIN(id) AS keep_id, COUNT(*) AS cnt "
           "FROM user WHERE phone_masked != '' "
           "GROUP BY phone_masked HAVING cnt > 1");
    while (q.next()) {
        const QString masked = q.value(0).toString();
        const int keepId = q.value(1).toInt();
        qDebug() << "[deduplicate] 发现重复用户" << masked << "保留 id=" << keepId;

        QSqlQuery q2(m_db);
        q2.prepare("SELECT id, balance FROM user WHERE phone_masked=? AND id!=?");
        q2.addBindValue(masked);
        q2.addBindValue(keepId);
        while (q2.next()) {
            const int dupId = q2.value(0).toInt();
            const double dupBalance = q2.value(1).toDouble();

            QSqlQuery uo(m_db);
            uo.prepare("UPDATE charge_order SET user_id=? WHERE user_id=?");
            uo.addBindValue(keepId); uo.addBindValue(dupId); uo.exec();

            QSqlQuery ur(m_db);
            ur.prepare("UPDATE recharge_log SET user_id=? WHERE user_id=?");
            ur.addBindValue(keepId); ur.addBindValue(dupId); ur.exec();

            QSqlQuery urv(m_db);
            urv.prepare("UPDATE charge_reservation SET user_id=? WHERE user_id=?");
            urv.addBindValue(keepId); urv.addBindValue(dupId); urv.exec();

            QSqlQuery ub(m_db);
            ub.prepare("UPDATE user SET balance = balance + ? WHERE id=?");
            ub.addBindValue(dupBalance); ub.addBindValue(keepId); ub.exec();

            QSqlQuery del(m_db);
            del.prepare("DELETE FROM user WHERE id=?");
            del.addBindValue(dupId); del.exec();

            qDebug() << "[deduplicate] 合并 id=" << dupId << "(余额" << dupBalance
                     << ")到 id=" << keepId << "并删除";
        }
    }
}

void DatabaseManager::seedDefaultData()
{
    QSqlQuery check(m_db);
    check.exec("SELECT COUNT(*) FROM admin");
    if (check.next() && check.value(0).toInt() == 0) {
        // 默认管理员 admin/123456 (加盐 SHA-256, 盐在前: SHA256(salt+pwd))
        const QString salt = "neusoft-admin-password-2026";
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(
                (salt + QString("123456")).toUtf8(), QCryptographicHash::Sha256).toHex());
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO admin(username, password_hash, salt) VALUES(?,?,?)");
        q.addBindValue("admin");
        q.addBindValue(hash);
        q.addBindValue(salt);
        q.exec();
    }

    check.exec("SELECT COUNT(*) FROM station");
    if (check.next() && check.value(0).toInt() > 0)
        return;   // 已有站点数据, 不重复种子

    struct SeedStation {
        const char *name; const char *addr;
        double lon; double lat; double price; int piles;
    };
    // 12 个北京城区充电站, 每站 6~10 个桩, 价格 0.8~1.6 元/度
    const SeedStation seeds[] = {
        { "东软望京充电站",       "北京市朝阳区望京街 10 号",     116.4810, 39.9978, 1.20, 8 },
        { "国贸 CBD 充电站",       "北京市朝阳区建国门外大街 1 号", 116.4615, 39.9087, 1.50, 10 },
        { "中关村软件园充电站",   "北京市海淀区中关村软件园 8 号楼",116.2982, 40.0489, 1.10, 8 },
        { "西单大悦城充电站",     "北京市西城区西单北大街 131 号", 116.3736, 39.9095, 1.60, 6 },
        { "北京南站枢纽充电站",   "北京市丰台区永外大街车站路",    116.3787, 39.8652, 1.30, 10 },
        { "亦庄经济开发区充电站", "北京市大兴区荣华中路 10 号",    116.5068, 39.7955, 0.90, 8 },
        { "五道口购物中心充电站", "北京市海淀区成府路 28 号",      116.3384, 39.9928, 1.30, 6 },
        { "奥林匹克公园充电站",   "北京市朝阳区北辰东路 15 号",    116.3975, 39.9919, 1.00, 8 },
        { "三里屯太古里充电站",   "北京市朝阳区三里屯路 19 号",    116.4552, 39.9370, 1.60, 6 },
        { "通州副中心充电站",     "北京市通州区运河西大街 2 号",   116.6586, 39.9025, 0.80, 8 },
        { "西直门交通枢纽充电站", "北京市西城区西直门外大街 1 号", 116.3553, 39.9408, 1.20, 10 },
        { "丰台科技园充电站",     "北京市丰台区科学城星火路 1 号", 116.2987, 39.8355, 0.95, 8 },
    };

    int codeSeq = 0;
    for (const SeedStation &s : seeds) {
        QSqlQuery sq(m_db);
        sq.prepare("INSERT INTO station(name, address, longitude, latitude, price) VALUES(?,?,?,?,?)");
        sq.addBindValue(QString::fromUtf8(s.name));
        sq.addBindValue(QString::fromUtf8(s.addr));
        sq.addBindValue(s.lon);
        sq.addBindValue(s.lat);
        sq.addBindValue(s.price);
        if (!sq.exec())
            continue;
        const int stationId = sq.lastInsertId().toInt();
        for (int i = 0; i < s.piles; ++i) {
            ++codeSeq;
            // 快慢交替: 偶数快充 60kW, 奇数慢充 7kW
            const bool fast = (i % 2 == 0);
            int status = PileIdle;
            if (codeSeq % 5 == 0)
                status = PileInUse;
            else if (codeSeq % 7 == 0)
                status = PileFault;
            QSqlQuery pq(m_db);
            pq.prepare("INSERT INTO pile(station_id, code, type, power, status) VALUES(?,?,?,?,?)");
            pq.addBindValue(stationId);
            pq.addBindValue(QString("P%1").arg(codeSeq, 4, 10, QChar('0')));
            pq.addBindValue(fast ? PileFast : PileSlow);
            pq.addBindValue(fast ? 60.0 : 7.0);
            pq.addBindValue(status);
            pq.exec();
        }
    }

    // 演示用户: 手机号 13800000001 (免密登录直接可用), 初始余额 200
    // 注意: 手机号必须与历史版本一致, 否则旧库演示订单会关联到错误用户
    QSqlQuery uq(m_db);
    uq.prepare("INSERT INTO user(phone, phone_masked, nickname, balance) VALUES(?,?,?,?)");
    uq.addBindValue(hashPhone("13800000001"));
    uq.addBindValue(maskPhone("13800000001"));
    uq.addBindValue("演示用户");
    uq.addBindValue(200.0);
    uq.exec();
}

void DatabaseManager::seedDefaultFeeRules()
{
    // 为每个站点生成默认分时费率(无规则时才生成, 管理端改过的不覆盖)
    // 谷段 00:00-07:00 / 23:00-24:00 = 基准价*0.8
    // 峰段 10:00-12:00 / 17:00-21:00 = 基准价*1.3
    // 平段 其余时间 = 基准价; 服务费统一 0.10 元/度
    struct Seg { int period; const char *start; const char *end; double mul; };
    const Seg segs[] = {
        { 0, "00:00", "07:00", 0.8 },
        { 1, "07:00", "10:00", 1.0 },
        { 2, "10:00", "12:00", 1.3 },
        { 1, "12:00", "17:00", 1.0 },
        { 2, "17:00", "21:00", 1.3 },
        { 1, "21:00", "23:00", 1.0 },
        { 0, "23:00", "24:00", 0.8 },
    };

    QSqlQuery stations(m_db);
    stations.exec("SELECT id, price FROM station");
    struct SRow { int id; double price; };
    QList<SRow> rows;
    while (stations.next())
        rows.append({ stations.value(0).toInt(), stations.value(1).toDouble() });

    for (const SRow &s : rows) {
        QSqlQuery cnt(m_db);
        cnt.prepare("SELECT COUNT(*) FROM price_rule WHERE station_id=?");
        cnt.addBindValue(s.id);
        cnt.exec();
        if (cnt.next() && cnt.value(0).toInt() > 0)
            continue;
        for (const Seg &seg : segs) {
            QSqlQuery ins(m_db);
            ins.prepare("INSERT INTO price_rule(station_id, period, start_time, end_time, price, service_fee)"
                        " VALUES(?,?,?,?,?,?)");
            ins.addBindValue(s.id);
            ins.addBindValue(seg.period);
            ins.addBindValue(QString::fromUtf8(seg.start));
            ins.addBindValue(QString::fromUtf8(seg.end));
            ins.addBindValue(qRound(s.price * seg.mul * 100) / 100.0);
            ins.addBindValue(0.10);
            ins.exec();
        }
    }
}

void DatabaseManager::seedDemoOrders()
{
    QSqlQuery check(m_db);
    check.exec("SELECT COUNT(*) FROM charge_order");
    if (check.next() && check.value(0).toInt() > 0)
        return;   // 已有订单不重复生成

    // 近 30 天演示订单(全部已完成), 让销售业绩页开箱即有数据
    QSqlQuery piles(m_db);
    piles.exec("SELECT id, station_id, power FROM pile WHERE status<>2 ORDER BY id");
    struct PRow { int id; int station; double power; };
    QList<PRow> pileRows;
    while (piles.next())
        pileRows.append({ piles.value(0).toInt(), piles.value(1).toInt(), piles.value(2).toDouble() });
    if (pileRows.isEmpty())
        return;

    // 演示用户 id 固定为首个用户
    QSqlQuery uq(m_db);
    uq.exec("SELECT id FROM user ORDER BY id LIMIT 1");
    if (!uq.next())
        return;
    const int userId = uq.value(0).toInt();

    QDateTime now = QDateTime::currentDateTime();
    int rng = 7;
    auto nextRand = [&rng](int mod) {
        rng = (rng * 1103515245 + 12345) & 0x7fffffff;
        return rng % mod;
    };

    for (int day = 29; day >= 0; --day) {
        const int orderCount = 3 + nextRand(6);   // 每天 3~8 单
        for (int k = 0; k < orderCount; ++k) {
            const PRow &p = pileRows[nextRand(pileRows.size())];
            const int minutes = 15 + nextRand(75);          // 15~89 分钟
            const double energy = p.power * minutes / 60.0; // 度
            QSqlQuery priceQ(m_db);
            priceQ.prepare("SELECT price FROM station WHERE id=?");
            priceQ.addBindValue(p.station);
            priceQ.exec();
            double price = 1.2;
            if (priceQ.next())
                price = priceQ.value(0).toDouble();
            const double amount = qRound(energy * price * 100) / 100.0;

            QDateTime start = now.addDays(-day).addSecs(-nextRand(86400));
            QDateTime end = start.addSecs(minutes * 60);
            QSqlQuery oq(m_db);
            oq.prepare("INSERT INTO charge_order(user_id, pile_id, station_id, start_time, end_time,"
                       " energy, amount, status, price_snapshot, sim_minutes, finish_type)"
                       " VALUES(?,?,?,?,?,?,?,1,?,?,0)");
            oq.addBindValue(userId);
            oq.addBindValue(p.id);
            oq.addBindValue(p.station);
            oq.addBindValue(start.toString("yyyy-MM-dd HH:mm:ss"));
            oq.addBindValue(end.toString("yyyy-MM-dd HH:mm:ss"));
            oq.addBindValue(qRound(energy * 100) / 100.0);
            oq.addBindValue(amount);
            oq.addBindValue(price);
            oq.addBindValue(minutes);
            oq.exec();

            // 累计桩使用次数/时长
            QSqlQuery pu(m_db);
            pu.prepare("UPDATE pile SET total_count=total_count+1, total_duration=total_duration+? WHERE id=?");
            pu.addBindValue(minutes);
            pu.addBindValue(p.id);
            pu.exec();
        }
    }
}
