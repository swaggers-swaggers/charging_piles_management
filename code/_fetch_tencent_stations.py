# -*- coding: utf-8 -*-
"""
从腾讯位置服务「地点搜索」API 抓取北京市真实充电站, 生成一个可直接被
ChargingServer 使用的 test.db(建表结构复刻 DatabaseManager.cpp)。

用法:
    python3 _fetch_tencent_stations.py <你的Key> [每区抓取条数]

示例:
    python3 _fetch_tencent_stations.py XXXXX-XXXXX-XXXXX 30

产物(写在当前目录, 即 code/):
    test.db               -> 完整数据库(真实站点 + 电桩)
    station_import.sql    -> 纯 SQL(便于核对 / 手动导入)
    seed_stations_cpp.txt -> C++ 种子数组(可选, 用于替换 DatabaseManager.cpp 里的硬编码)

原理:
    boundary=region(城市名, 0) 按行政区搜索; keyword=充电站; 每页 20 条, 翻页抓取。
    对每个站点按名称去重, 并确定性生成若干电桩(快充/慢充 + 闲置/在用/故障)。
    电桩价格按站点确定性取 0.9~1.6 元/度(演示用, 非真实价格)。

注意:
    - 需要能访问 https://apis.map.qq.com
    - Key 需为 WebServiceAPI 类型, 且授权方式匹配(授权IP 或 签名校验)
    - 导入前请先删除旧的 test.db(否则站点会叠加), 或直接用本脚本生成的新 test.db
"""
import json
import sys
import hashlib
import socket
import sqlite3
import ssl
import urllib.parse
import urllib.request
import urllib.error

# macOS 自带 Python 常缺 CA 证书, 导致 SSL 校验失败; 优先用 certifi 提供的证书
def _ssl_context():
    try:
        import certifi
        return ssl.create_default_context(cafile=certifi.where())
    except Exception:  # noqa: BLE001
        return ssl.create_default_context()

# 强制走 IPv4: 避免本机同时有 IPv6 时, 请求走 IPv6 导致授权 IP(IPv4)不匹配
def _force_ipv4():
    _orig_getaddrinfo = socket.getaddrinfo
    def _ipv4_getaddrinfo(*args, **kwargs):
        return [r for r in _orig_getaddrinfo(*args, **kwargs) if r[0] == socket.AF_INET]
    socket.getaddrinfo = _ipv4_getaddrinfo

API = "https://apis.map.qq.com/ws/place/v1/search"

# 要抓取的北京行政区(可自行增删; 数量多会拉长抓取时间并受 API 配额限制)
DISTRICTS = [
    "海淀区",
    "朝阳区",
    "东城区",
    "西城区",
    "丰台区",
    "通州区",
    "大兴区",
]

KEYWORD = "充电站"
PAGE_SIZE = 20  # 腾讯地点搜索单页上限 20


def fetch_page(key, keyword, region, page_index, page_size):
    params = {
        "keyword": keyword,
        "boundary": "region(%s,0)" % region,
        "key": key,
        "page_size": str(page_size),
        "page_index": str(page_index),
    }
    url = API + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "ChargingPlatform/1.0"})
    with urllib.request.urlopen(req, timeout=15, context=_ssl_context()) as resp:
        return json.loads(resp.read().decode("utf-8"))


def fetch_district(key, district, limit):
    """抓取单个行政区的充电站, 返回去重后的 POI 列表(每项含 title/address/lng/lat)。"""
    seen = set()
    out = []
    for page in range(1, 11):  # 最多翻 10 页(200 条), 通常足够了
        try:
            data = fetch_page(key, KEYWORD, district, page, PAGE_SIZE)
        except urllib.error.URLError as e:
            print("  [!] 网络请求失败: %s" % e)
            break
        except Exception as e:  # noqa: BLE001
            print("  [!] 解析失败: %s" % e)
            break

        if data.get("status") != 0:
            print("  [!] 腾讯返回错误 status=%s message=%s"
                  % (data.get("status"), data.get("message")))
            break

        pois = data.get("data") or []
        if not pois:
            break

        for p in pois:
            pid = p.get("id")
            title = (p.get("title") or "").strip()
            loc = p.get("location") or {}
            lng = loc.get("lng")
            lat = loc.get("lat")
            if not title or lng is None or lat is None:
                continue
            if pid in seen:
                continue
            seen.add(pid)
            out.append({
                "name": title,
                "address": (p.get("address") or "").strip() or "北京市" + district,
                "lng": float(lng),
                "lat": float(lat),
            })
            if len(out) >= limit:
                return out
        print("  抓取 %s 第 %d 页: 累计 %d 条" % (district, page, len(out)))
    return out


def norm_name(name):
    # 去掉 "(充电站)" / "(特来电)" 之类后缀, 让站名更干净
    if "(" in name:
        name = name.split("(")[0]
    return name.strip()


def gen_piles(station_index):
    """按站点序号确定性生成电桩(数量/类型/状态可复现, 演示用)。"""
    h = int(hashlib.md5(str(station_index).encode()).hexdigest(), 16)
    count = 4 + h % 9          # 4~12 根
    piles = []
    for i in range(count):
        seq = station_index * 1000 + i
        ptype = 0 if (seq % 3 != 0) else 1   # 多数快充, 少数慢充
        power = 60.0 if ptype == 0 else 7.0  # kW
        r = (h + i * 131) % 20
        if r == 0:
            status = 2           # 故障
        elif r <= 5:
            status = 1           # 在用
        else:
            status = 0           # 闲置
        piles.append({
            "code": "CP-%03d" % (seq % 1000),
            "type": ptype,
            "power": power,
            "status": status,
        })
    return piles


def price_for(station_index):
    h = int(hashlib.md5(str(station_index).encode()).hexdigest(), 16)
    return round(0.9 + (h % 8) * 0.1, 2)   # 0.9 ~ 1.6 元/度


def main():
    _force_ipv4()  # 必须在发起任何请求前调用
    if len(sys.argv) < 2:
        print(__doc__)
        print("缺少参数: 请传入腾讯地图 Key")
        sys.exit(1)
    key = sys.argv[1].strip()
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 30

    stations = []
    for d in DISTRICTS:
        print("== 抓取 %s ==" % d)
        got = fetch_district(key, d, limit)
        stations.extend(got)
        if len(stations) >= limit * len(DISTRICTS):
            break

    # 全城再去重一次(不同区可能搜到同一站)
    seen_name = set()
    unique = []
    for s in stations:
        nm = norm_name(s["name"])
        if not nm or nm in seen_name:
            continue
        seen_name.add(nm)
        s["name"] = nm
        unique.append(s)

    if not unique:
        print("没有抓到任何站点, 请检查 Key / 授权方式 / 网络")
        sys.exit(1)

    print("\n共抓到 %d 个真实充电站, 开始生成 test.db ..." % len(unique))

    # ---------- 建表(复刻 DatabaseManager.cpp createTables) ----------
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

    con = sqlite3.connect("test.db")
    cur = con.cursor()
    for stmt in SCHEMA:
        cur.execute(stmt)

    sql_lines = []
    cpp_lines = []

    # admin 用户(与 C++ 一致的 MD5 密码)
    cur.execute("INSERT OR IGNORE INTO admin (username, password) VALUES (?, ?)",
                ("admin", hashlib.md5(b"123456").hexdigest()))

    for idx, s in enumerate(unique):
        price = price_for(idx)
        cur.execute(
            "INSERT INTO station (name, address, longitude, latitude, price) VALUES (?,?,?,?,?)",
            (s["name"], s["address"], s["lng"], s["lat"], price),
        )
        station_id = cur.lastrowid

        sql_lines.append(
            "INSERT INTO station (name, address, longitude, latitude, price) VALUES (%s,%s,%s,%s,%s);"
            % tuple(json.dumps(x, ensure_ascii=False) for x in
                    (s["name"], s["address"], s["lng"], s["lat"], price)))
        cpp_lines.append(
            '        { "%s", "%s", %.6f, %.6f, %.2f, %d },'
            % (s["name"], s["address"], s["lng"], s["lat"], price,
               len(gen_piles(idx))))

        for p in gen_piles(idx):
            cur.execute(
                "INSERT INTO pile (station_id, code, type, power, status) VALUES (?,?,?,?,?)",
                (station_id, p["code"], p["type"], p["power"], p["status"]),
            )
            sql_lines.append(
                "INSERT INTO pile (station_id, code, type, power, status) VALUES (%d,%s,%d,%s,%d);"
                % (station_id, json.dumps(p["code"], ensure_ascii=False),
                   p["type"], p["power"], p["status"]))

    con.commit()

    # 演示用户: 与 C++ 一致的手机号哈希 + 脱敏(保证客户端登录可用)
    SALT = b"neusoft-charging-platform-2026"
    demo_users = [
        ("13800000001", "演示用户A", 500.0),
        ("13911112222", "演示用户B", 260.0),
        ("13733334444", "演示用户C", 320.0),
    ]
    for phone, nick, bal in demo_users:
        masked = phone[:3] + "****" + phone[-4:]
        h = hashlib.sha256(SALT + phone.encode("utf-8")).hexdigest()
        cur.execute(
            "INSERT OR IGNORE INTO user (phone, phone_masked, nickname, balance) VALUES (?,?,?,?)",
            (h, masked, nick, bal),
        )
    con.commit()
    con.close()

    with open("station_import.sql", "w", encoding="utf-8") as f:
        f.write("\n".join(sql_lines) + "\n")

    with open("seed_stations_cpp.txt", "w", encoding="utf-8") as f:
        f.write("// 以下数组由 _fetch_tencent_stations.py 生成, 直接替换\n")
        f.write("// DatabaseManager.cpp 中 seedDefaultData() 的 seeds[] 大括号内容即可。\n")
        f.write("\n".join(cpp_lines) + "\n")

    print("完成:")
    print("  站点: %d 个" % len(unique))
    print("  数据库: test.db")
    print("  SQL:    station_import.sql")
    print("  C++:    seed_stations_cpp.txt (可选, 用于替换 DatabaseManager.cpp 的硬编码种子)")
    print("\n下一步: 删除旧 test.db, 直接运行服务端即可(本脚本生成的新 test.db 会被直接使用)。")


if __name__ == "__main__":
    main()
