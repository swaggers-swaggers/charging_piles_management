import sqlite3
conn = sqlite3.connect(r'D:\共享文件夹\Projects\database\test.db')
cur = conn.cursor()
print('=== admin 表结构 ===')
cur.execute("PRAGMA table_info(admin)")
for row in cur.fetchall():
    print(row)
print()
print('=== admin 表数据 ===')
try:
    cur.execute('SELECT * FROM admin')
    cols = [d[0] for d in cur.description]
    print('列:', cols)
    for row in cur.fetchall():
        print(row)
except Exception as e:
    print('查询失败:', e)
print()
print('=== user 表结构(前几列) ===')
cur.execute("PRAGMA table_info(user)")
for row in cur.fetchall():
    print(row)
print()
print('=== 所有表 ===')
cur.execute("SELECT name FROM sqlite_master WHERE type='table'")
for row in cur.fetchall():
    print(row[0])
conn.close()
