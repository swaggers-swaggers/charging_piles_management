-- =====================================================================
-- 充电系统 v2 数据库升级脚本（upgrade_charging_v2.sql）
-- 适用：东软电动汽车充电桩应用管理平台（SQLite）
-- 用途：新增 price_rule / charge_reservation / recharge_log 三张表，
--       为 charge_order 扩展充电目标、冻结、计费快照、结束原因等列，
--       为演示充电站生成默认峰谷平分时费率。
--
-- 执行方式（Ubuntu 虚拟机）：
--   sqlite3 database/test.db < database/upgrade_charging_v2.sql
--
-- 幂等性说明：
--   新表 / 索引 / 费率种子均使用 IF NOT EXISTS / 条件插入，可重复执行；
--   charge_order 的 ALTER TABLE 不支持 IF NOT EXISTS，重复执行会报
--   "duplicate column name"，属正常现象，可忽略；更稳妥的方式是
--   直接启动 ChargingServer（DatabaseManager 会自动完成同样的升级）。
-- =====================================================================

-- ---------------------------------------------------------------------
-- 1. 新表：分时费率表（每站 3 档：峰/平/谷）
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS price_rule (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id  INTEGER NOT NULL,
    period      INTEGER NOT NULL DEFAULT 0,   -- 0=低谷 1=平段 2=高峰
    start_time  TEXT NOT NULL DEFAULT '00:00',
    end_time    TEXT NOT NULL DEFAULT '24:00',
    price       REAL NOT NULL DEFAULT 1.0,    -- 电价（元/度）
    service_fee REAL NOT NULL DEFAULT 0.0,    -- 服务费（元/度）
    FOREIGN KEY(station_id) REFERENCES station(id)
);

-- 2. 新表：排队/预约表（type 区分：0=排队 1=预约；预约含时段字段）
CREATE TABLE IF NOT EXISTS charge_reservation (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL,
    pile_id       INTEGER NOT NULL,
    station_id    INTEGER NOT NULL,
    type          INTEGER NOT NULL DEFAULT 0, -- 0=排队 1=预约
    create_time   TEXT DEFAULT (datetime('now','localtime')),
    assign_time   TEXT,                       -- 排队：引擎分配（轮到）时间
    expire_time   TEXT,                       -- 排队：分配后确认截止时间
    reserve_date  TEXT,                       -- 预约：日期 yyyy-MM-dd
    reserve_start TEXT,                       -- 预约：开始时刻 HH:MM
    reserve_end   TEXT,                       -- 预约：结束时刻 HH:MM
    status        INTEGER DEFAULT 0,          -- 0=有效(排队中/预约待履约) 1=已分配/已到桩
                                              -- 2=已取消 3=已过期 4=已履约(转订单)
    FOREIGN KEY(user_id) REFERENCES user(id),
    FOREIGN KEY(pile_id) REFERENCES pile(id),
    FOREIGN KEY(station_id) REFERENCES station(id)
);

-- 3. 新表：充值流水表
CREATE TABLE IF NOT EXISTS recharge_log (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL,
    amount        REAL NOT NULL,
    balance_after REAL NOT NULL,
    create_time   TEXT DEFAULT (datetime('now','localtime')),
    FOREIGN KEY(user_id) REFERENCES user(id)
);

-- ---------------------------------------------------------------------
-- 4. charge_order 扩展列（重复执行会报 duplicate column，可忽略）
-- ---------------------------------------------------------------------
ALTER TABLE charge_order ADD COLUMN freeze_amount  REAL DEFAULT 0;   -- 预授权冻结金额
ALTER TABLE charge_order ADD COLUMN target_type    INTEGER DEFAULT 0;-- 0=不限 1=电量 2=金额 3=时长
ALTER TABLE charge_order ADD COLUMN target_value   REAL DEFAULT 0;
ALTER TABLE charge_order ADD COLUMN price_snapshot REAL DEFAULT 0;   -- 计费电价快照(含服务费)
ALTER TABLE charge_order ADD COLUMN finish_type    INTEGER DEFAULT 0;-- 0=用户 1=目标 2=余额耗尽 4=管理员 5=故障
ALTER TABLE charge_order ADD COLUMN cancel_reason  TEXT DEFAULT '';
ALTER TABLE charge_order ADD COLUMN refund_amount  REAL DEFAULT 0;

-- 4.1 station / pile 追加"是否开放预约"列（重复执行会报 duplicate column，可忽略）
ALTER TABLE station ADD COLUMN allow_reserve INTEGER DEFAULT 1;
ALTER TABLE pile    ADD COLUMN allow_reserve INTEGER DEFAULT 1;

-- ---------------------------------------------------------------------
-- 5. 索引（新表查询加速）
-- ---------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_reservation_pile  ON charge_reservation(pile_id, status);
CREATE INDEX IF NOT EXISTS idx_reservation_user  ON charge_reservation(user_id, status);
CREATE INDEX IF NOT EXISTS idx_price_rule_station ON price_rule(station_id);
CREATE INDEX IF NOT EXISTS idx_recharge_log_user  ON recharge_log(user_id);
CREATE INDEX IF NOT EXISTS idx_order_status       ON charge_order(status);
CREATE INDEX IF NOT EXISTS idx_order_pile         ON charge_order(pile_id, status);

-- ---------------------------------------------------------------------
-- 6. 为尚无费率配置的充电站生成默认峰谷平分时费率（幂等）
--    谷 23:00-07:00 = station.price * 0.8
--    平 其余        = station.price
--    峰 10:00-12:00, 17:00-21:00 = station.price * 1.3
--    服务费统一 0.10 元/度（演示）
-- ---------------------------------------------------------------------
INSERT INTO price_rule (station_id, period, start_time, end_time, price, service_fee)
SELECT s.id, 2, '10:00', '12:00', ROUND(s.price * 1.3, 2), 0.10
FROM station s
WHERE NOT EXISTS (SELECT 1 FROM price_rule r WHERE r.station_id = s.id AND r.period = 2 AND r.start_time = '10:00');

INSERT INTO price_rule (station_id, period, start_time, end_time, price, service_fee)
SELECT s.id, 2, '17:00', '21:00', ROUND(s.price * 1.3, 2), 0.10
FROM station s
WHERE NOT EXISTS (SELECT 1 FROM price_rule r WHERE r.station_id = s.id AND r.period = 2 AND r.start_time = '17:00');

INSERT INTO price_rule (station_id, period, start_time, end_time, price, service_fee)
SELECT s.id, 0, '23:00', '07:00', ROUND(s.price * 0.8, 2), 0.10
FROM station s
WHERE NOT EXISTS (SELECT 1 FROM price_rule r WHERE r.station_id = s.id AND r.period = 0);

INSERT INTO price_rule (station_id, period, start_time, end_time, price, service_fee)
SELECT s.id, 1, '07:00', '10:00', s.price, 0.10
FROM station s
WHERE NOT EXISTS (SELECT 1 FROM price_rule r WHERE r.station_id = s.id AND r.period = 1 AND r.start_time = '07:00');

INSERT INTO price_rule (station_id, period, start_time, end_time, price, service_fee)
SELECT s.id, 1, '12:00', '17:00', s.price, 0.10
FROM station s
WHERE NOT EXISTS (SELECT 1 FROM price_rule r WHERE r.station_id = s.id AND r.period = 1 AND r.start_time = '12:00');

INSERT INTO price_rule (station_id, period, start_time, end_time, price, service_fee)
SELECT s.id, 1, '21:00', '23:00', s.price, 0.10
FROM station s
WHERE NOT EXISTS (SELECT 1 FROM price_rule r WHERE r.station_id = s.id AND r.period = 1 AND r.start_time = '21:00');

-- 校验：查看生成结果
-- SELECT station_id, period, start_time, end_time, price, service_fee FROM price_rule ORDER BY station_id, start_time;
