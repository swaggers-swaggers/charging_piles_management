import sqlite3, hashlib
conn = sqlite3.connect(r'D:\共享文件夹\Projects\database\test.db')
cur = conn.cursor()
print("=== user 表结构 ===")
cur.execute("PRAGMA table_info(user)")
for r in cur.fetchall():
    print(r)
print()
print("=== user 表全部数据 ===")
cur.execute("SELECT id, phone, phone_masked, nickname, balance, status, register_time FROM user")
for r in cur.fetchall():
    print(r)
print()
print("=== 哈希验证 ===")
for phone in ["13800000000"]:
    h_nosalt = hashlib.sha256(phone.encode()).hexdigest()
    h_salt1 = hashlib.sha256(("neusoft-charging-platform-2026"+phone).encode()).hexdigest()
    print(f"  {phone}:")
    print(f"    无盐:       {h_nosalt}")
    print(f"    加盐(旧):   {h_salt1}")
conn.close()
