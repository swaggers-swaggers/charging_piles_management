import sqlite3, hashlib, shutil
src = r'D:\共享文件夹\Projects\database\test.db'
dst = r'D:\共享文件夹\Projects\_buildcheck\test_migrate.db'
shutil.copy(src, dst)
conn = sqlite3.connect(dst)
cur = conn.cursor()

cur.execute("PRAGMA table_info(admin)")
cols = [r[1] for r in cur.fetchall()]
print("迁移前列:", cols)

if "password" in cols and "password_hash" not in cols:
    cur.execute("ALTER TABLE admin RENAME COLUMN password TO password_hash")
cur.execute("PRAGMA table_info(admin)")
cols2 = [r[1] for r in cur.fetchall()]
if "salt" not in cols2:
    cur.execute("ALTER TABLE admin ADD COLUMN salt TEXT DEFAULT ''")
    cur.execute("UPDATE admin SET salt='neusoft-admin-password-2026' WHERE salt=''")
conn.commit()

cur.execute("SELECT id,username,password_hash,salt FROM admin")
row = cur.fetchone()
print("迁移后行:", row)
stored = row[2]
salt = row[3]
for pwd in ["123456", "admin123"]:
    calc = hashlib.sha256((salt + pwd).encode()).hexdigest()
    print(f"  密码 {pwd}: 匹配={calc == stored}")
conn.close()
print("模拟迁移+校验完成")
