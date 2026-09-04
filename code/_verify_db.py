# -*- coding: utf-8 -*-
import sqlite3, hashlib
con = sqlite3.connect(r'D:\lesson\电\code\test.db')
cur = con.cursor()
print('== 表 ==')
for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'"):
    print(' ', r[0])
print('== 电桩状态分布 (0闲置/1在用/2故障) ==')
for r in cur.execute('SELECT status, COUNT(*) FROM pile GROUP BY status ORDER BY status'):
    print('  status', r[0], '=', r[1])
print('== 电桩类型 (0快充/1慢充) ==')
for r in cur.execute('SELECT type, COUNT(*) FROM pile GROUP BY type'):
    print('  type', r[0], '=', r[1])
print('== 用户示例 ==')
for r in cur.execute('SELECT phone_masked, nickname, balance, status FROM user LIMIT 4'):
    print(' ', r)
salt = b'neusoft-charging-platform-2026'
h = hashlib.sha256(salt + b'13800000001').hexdigest()
row = cur.execute("SELECT phone, phone_masked FROM user WHERE phone_masked='138****0001'").fetchone()
print('== 13800000001 哈希匹配 ==', (row is not None and row[0] == h))
print('== 订单汇总(近30天) ==')
r = cur.execute('SELECT ROUND(SUM(amount),2), ROUND(SUM(energy),2), ROUND(AVG(amount),2), COUNT(*) FROM charge_order').fetchone()
print('  总营收=%s元 总电量=%skWh 平均单笔=%s元 笔数=%s' % r)
print('== 今日营收 ==')
r = cur.execute("SELECT ROUND(SUM(amount),2) FROM charge_order WHERE date(start_time)=date('now','localtime')").fetchone()
print('  ', r[0])
print('== 本月营收 ==')
r = cur.execute("SELECT ROUND(SUM(amount),2) FROM charge_order WHERE strftime('%Y-%m',start_time)=strftime('%Y-%m','now','localtime')").fetchone()
print('  ', r[0])
print('== 站点坐标抽样 ==')
for r in cur.execute('SELECT name, longitude, latitude, price FROM station LIMIT 5'):
    print(' ', r)
con.close()
print('验证完成')
