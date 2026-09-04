# -*- coding: utf-8 -*-
"""
生成 code/test.db: 复刻 DatabaseManager.cpp 的建表 + 北京站点种子 + 演示用户 + 近30天订单。
手机号哈希算法与 C++ 完全一致(盐 "neusoft-charging-platform-2026" + SHA-256)。
订单 LCG 种子 20260904, 与 C++ 一致, 结果确定可复现。
"""
import hashlib, math, sqlite3, datetime

DB_PATH = r'D:\lesson\电\code\test.db'

SALT = b'neusoft-charging-platform-2026'

def hash_phone(phone):
    return hashlib.sha256(SALT + phone.encode('utf-8')).hexdigest()

def mask_phone(phone):
    # 与 C++ maskPhone 一致: 前3 + **** + 后4
    return phone[:3] + '****' + phone[-4:] if len(phone) > 7 else phone[:3] + '****'

def md5(s):
    return hashlib.md5(s.encode('utf-8')).hexdigest()

# ---------------- 建表(与 C++ createTables 一致) ----------------
SCHEMA = [
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
]

# ---------------- 北京真实站点种子(与 C++ seeds[] 一致) ----------------
STATIONS = [
    ("特来电五道口充电站",       "北京市海淀区成府路28号五道口购物中心停车场",    116.339065, 39.991117, 1.35, 8),
    ("开迈斯国家体育馆充电站",   "北京市朝阳区天辰东路9号奥林匹克公园P3停车场",    116.387746, 39.997471, 1.60, 12),
    ("中石化奥林匹克P2充电站",   "北京市朝阳区天辰西路水立方停车场",               116.387679, 39.993433, 1.20, 6),
    ("昆仑网电望京南充电站",     "北京市朝阳区望京南加油站",                       116.482330, 40.013817, 1.25, 8),
    ("国家电网大兴机场充电站",   "北京市大兴区天兴一街与航兴路交叉口南侧停车场",   116.420394, 39.529542, 1.50, 10),
    ("小桔充电望京文化产业园站", "北京市朝阳区望京西路48-6号",                     116.479208, 39.994492, 1.10, 6),
    ("普天望京凯德Mall充电站",   "北京市朝阳区广顺北大街33号凯德Mall停车场",       116.468897, 39.992102, 1.15, 4),
    ("国家电网北京坊充电站",     "北京市西城区大栅栏煤市街北京坊B3停车场",         116.396702, 39.898266, 1.30, 6),
    ("昆仑网电工体西门充电站",   "北京市东城区新中街东直门城市生态岛旁",           116.439840, 39.931522, 1.45, 8),
    ("比亚迪通州科创充电站",     "北京市通州区科创东五街1号",                       116.551189, 39.813862, 1.05, 10),
    ("小桔充电亚林西充电站",     "北京市丰台区南苑亚林西",                          116.350235, 39.854170, 0.95, 6),
    ("高陆通成铭大厦充电站",     "北京市西城区西直门南大街2号成铭大厦B4停车场",    116.350637, 39.938143, 1.20, 4),
]

# ---------------- 演示用户(与 C++ kDemoUsers[] 一致) ----------------
DEMO_USERS = [
    ("13800000001", "演示用户A", 500.0, 90),
    ("13911112222", "演示用户B", 260.0, 75),
    ("13733334444", "演示用户C", 320.0, 60),
    ("13655556666", "演示用户D", 150.0, 45),
    ("13577778888", "演示用户E", 88.5, 30),
    ("15899990001", "演示用户F", 410.0, 80),
    ("15911112222", "演示用户G", 60.0, 20),
    ("18633334444", "演示用户H", 200.0, 55),
    ("18855556666", "演示用户I", 700.0, 100),
    ("18577778888", "演示用户J", 35.0, 10),
]

# 24h 权重(与 C++ hourWeight 一致)
HOUR_WEIGHT = [1,1,1,1,2,4, 8,12,15,12,10,10, 9,9,10,11,13,16, 18,18,15,11,7,3]
WEIGHT_SUM = sum(HOUR_WEIGHT)

class LCG:
    def __init__(self, seed):
        self.seed = seed
    def rnd(self):
        self.seed = (self.seed * 1103515245 + 12345) & 0xFFFFFFFF
        return (self.seed >> 16) & 0x7fff

def main():
    today = datetime.date.today()
    rng = LCG(20260904)
    rnd = rng.rnd

    con = sqlite3.connect(DB_PATH)
    cur = con.cursor()
    # 幂等: 重建前先清空旧表(避免重复运行数据翻倍)
    for t in ('charge_order', 'pile', 'station', 'op_log', 'user', 'admin'):
        cur.execute(f'DROP TABLE IF EXISTS {t}')
    cur.execute("DELETE FROM sqlite_sequence")
    for s in SCHEMA:
        cur.execute(s)

    # admin
    cur.execute("INSERT OR IGNORE INTO admin (username, password) VALUES (?, ?)", ("admin", md5("123456")))

    # 站点 + 电桩
    piles = []  # (id, station_id, type, power)
    code_seq = 1
    for name, addr, lng, lat, price, n in STATIONS:
        cur.execute("INSERT INTO station (name, address, longitude, latitude, price) VALUES (?,?,?,?,?)",
                    (name, addr, lng, lat, price))
        sid = cur.lastrowid
        for i in range(n):
            ptype = 0 if i % 2 == 0 else 1   # 0=快充 1=慢充
            power = 60.0 if ptype == 0 else 7.0
            status = 0
            if code_seq % 5 == 0: status = 1
            if code_seq % 7 == 0: status = 2
            code = "CP-%03d" % code_seq
            cur.execute("INSERT INTO pile (station_id, code, type, power, status) VALUES (?,?,?,?,?)",
                        (sid, code, ptype, power, status))
            piles.append((cur.lastrowid, sid, ptype, power))
            code_seq += 1

    # 演示用户
    user_ids = []
    for phone, nick, bal, reg in DEMO_USERS:
        cur.execute("SELECT id FROM user WHERE phone = ?", (hash_phone(phone),))
        row = cur.fetchone()
        if row:
            uid = row[0]
        else:
            rt = (today - datetime.timedelta(days=reg)).strftime("%Y-%m-%d %H:%M:%S")
            cur.execute("INSERT INTO user (phone, phone_masked, nickname, balance, register_time) VALUES (?,?,?,?,?)",
                        (hash_phone(phone), mask_phone(phone), nick, bal, rt))
            uid = cur.lastrowid
        user_ids.append(uid)

    # 清空演示用户旧订单(全新库无需, 幂等处理)
    for uid in user_ids:
        cur.execute("DELETE FROM charge_order WHERE user_id = ?", (uid,))

    # 生成近30天订单
    station_price = {}
    cur.execute("SELECT id, price FROM station")
    for sid, p in cur.fetchall():
        station_price[sid] = p

    total_orders = 0
    for day in range(29, -1, -1):
        d = today - datetime.timedelta(days=day)
        weekend = d.isoweekday() in (6, 7)
        n_orders = (22 + rnd() % 24) if weekend else (18 + rnd() % 22)
        for _ in range(n_orders):
            ui = rnd() % len(user_ids)
            pl = piles[rnd() % len(piles)]
            pid, sid, ptype, power = pl
            price = station_price[sid]
            # pickHour
            r = rnd() % WEIGHT_SUM
            hour = 18
            for h, w in enumerate(HOUR_WEIGHT):
                if r < w:
                    hour = h
                    break
                r -= w
            minute = rnd() % 60
            if ptype == 0:
                energy = 15.0 + rnd() % 41
                duration = 0.4 + (rnd() % 12) / 10.0
            else:
                energy = 10.0 + rnd() % 31
                duration = 2.0 + (rnd() % 61) / 10.0
            amount = round(energy * price * 100.0) / 100.0
            start = d.strftime("%Y-%m-%d") + " %02d:%02d:00" % (hour, minute)
            start_dt = datetime.datetime.strptime(start, "%Y-%m-%d %H:%M:%S")
            end_dt = start_dt + datetime.timedelta(hours=duration)
            end = end_dt.strftime("%Y-%m-%d %H:%M:%S")
            cur.execute("INSERT INTO charge_order (user_id, pile_id, station_id, start_time, end_time, energy, amount, status) VALUES (?,?,?,?,?,?,?,1)",
                        (user_ids[ui], pid, sid, start, end, energy, amount))
            total_orders += 1

    con.commit()

    # ---------------- 验证 ----------------
    def q(sql):
        return cur.execute(sql).fetchall()
    n_station = len(q("SELECT id FROM station"))
    n_pile = len(q("SELECT id FROM pile"))
    n_user = len(q("SELECT id FROM user"))
    n_order = len(q("SELECT id FROM charge_order"))
    n_order_today = len(q("SELECT id FROM charge_order WHERE date(start_time)=date('now','localtime')"))
    print(f"stations={n_station} piles={n_pile} users={n_user} orders={total_orders}(今日{n_order_today})")
    # 抽样: 每站订单数
    for row in q("SELECT s.name, COUNT(o.id) FROM station s LEFT JOIN charge_order o ON o.station_id=s.id GROUP BY s.id ORDER BY s.id"):
        print("  ", row[0], row[1], "单")
    # 每小时订单分布(近30天)
    hourly = {}
    for (h,) in q("SELECT CAST(strftime('%H', start_time) AS INT) FROM charge_order"):
        hourly[h] = hourly.get(h, 0) + 1
    peak = sorted(hourly.items(), key=lambda x: -x[1])[:3]
    print("24h分布(近30天):", {k: hourly[k] for k in sorted(hourly)})
    print("高峰时段:", peak)
    con.close()
    print("OK ->", DB_PATH)

if __name__ == "__main__":
    main()
