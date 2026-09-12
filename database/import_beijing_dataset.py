#!/usr/bin/env python3
"""将北京模拟充电数据集完整导入项目 SQLite 分析表。

导入表使用 analytics_beijing_ 前缀，不覆盖 ChargingPlatform 原有业务表。
脚本默认在导入前使用 SQLite backup API 创建可恢复备份，并重建分析表。
"""

from __future__ import annotations

import argparse
import csv
import io
import sqlite3
import sys
import time
import zipfile
from datetime import datetime
from pathlib import Path


TABLES = (
    "analytics_beijing_bms",
    "analytics_beijing_snapshot",
    "analytics_beijing_session",
    "analytics_beijing_weather",
    "analytics_beijing_station",
)


SCHEMA_SQL = """
CREATE TABLE analytics_beijing_station (
    station_id       INTEGER PRIMARY KEY,
    location_id      TEXT NOT NULL,
    facility_type    INTEGER NOT NULL CHECK (facility_type BETWEEN 1 AND 4),
    station_name     TEXT NOT NULL,
    address          TEXT NOT NULL,
    device_count     INTEGER NOT NULL CHECK (device_count >= 0),
    open_time        TEXT NOT NULL,
    update_time      TEXT NOT NULL
);

CREATE TABLE analytics_beijing_weather (
    date              TEXT PRIMARY KEY,
    weekday           INTEGER NOT NULL,
    holiday           INTEGER NOT NULL CHECK (holiday IN (0, 1)),
    temp_high         REAL NOT NULL,
    temp_low          REAL NOT NULL,
    condition         INTEGER NOT NULL,
    precipitation     REAL NOT NULL
) WITHOUT ROWID;

CREATE TABLE analytics_beijing_session (
    session_id       INTEGER PRIMARY KEY,
    kwh_total        REAL NOT NULL,
    charging_fees    REAL NOT NULL,
    created          TEXT NOT NULL,
    ended            TEXT NOT NULL,
    start_hour       INTEGER NOT NULL,
    end_hour         INTEGER NOT NULL,
    charge_time_hrs  REAL NOT NULL,
    weekday          TEXT NOT NULL,
    platform         TEXT NOT NULL,
    user_id          INTEGER NOT NULL,
    station_id       INTEGER NOT NULL,
    location_id      TEXT NOT NULL,
    manager_vehicle  INTEGER NOT NULL,
    facility_type    INTEGER NOT NULL,
    mon               INTEGER NOT NULL,
    tues              INTEGER NOT NULL,
    wed               INTEGER NOT NULL,
    thurs             INTEGER NOT NULL,
    fri               INTEGER NOT NULL,
    sat               INTEGER NOT NULL,
    sun               INTEGER NOT NULL,
    FOREIGN KEY (station_id) REFERENCES analytics_beijing_station(station_id)
);

CREATE TABLE analytics_beijing_snapshot (
    station_id        INTEGER NOT NULL,
    record_time       TEXT NOT NULL,
    in_use            INTEGER NOT NULL,
    idle              INTEGER NOT NULL,
    fault             INTEGER NOT NULL,
    charging_power_kw REAL NOT NULL,
    queue_count       INTEGER NOT NULL,
    cum_kwh_day       REAL NOT NULL,
    PRIMARY KEY (station_id, record_time),
    FOREIGN KEY (station_id) REFERENCES analytics_beijing_station(station_id)
) WITHOUT ROWID;

CREATE TABLE analytics_beijing_bms (
    esd                    INTEGER NOT NULL,
    record_time            TEXT NOT NULL,
    soc                    REAL NOT NULL,
    pack_voltage_v         REAL NOT NULL,
    charge_current_a       REAL NOT NULL,
    max_cell_voltage_v     REAL NOT NULL,
    min_cell_voltage_v     REAL NOT NULL,
    max_temperature_c      REAL NOT NULL,
    min_temperature_c      REAL NOT NULL,
    available_energy_kw    REAL NOT NULL,
    available_capacity_ah  REAL NOT NULL,
    PRIMARY KEY (esd, record_time),
    FOREIGN KEY (esd) REFERENCES analytics_beijing_session(session_id)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS analytics_beijing_import_log (
    table_name       TEXT PRIMARY KEY,
    source_member    TEXT NOT NULL,
    row_count        INTEGER NOT NULL,
    imported_at      TEXT NOT NULL,
    elapsed_seconds  REAL NOT NULL,
    status           TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS analytics_beijing_dataset_meta (
    dataset_name     TEXT PRIMARY KEY,
    source_zip       TEXT NOT NULL,
    data_nature      TEXT NOT NULL,
    min_date         TEXT,
    max_date         TEXT,
    imported_at      TEXT NOT NULL,
    notes            TEXT NOT NULL
);
"""


INDEX_SQL = """
CREATE INDEX idx_beijing_session_created ON analytics_beijing_session(created);
CREATE INDEX idx_beijing_session_station_created ON analytics_beijing_session(station_id, created);
CREATE INDEX idx_beijing_session_user_created ON analytics_beijing_session(user_id, created);
CREATE INDEX idx_beijing_snapshot_time ON analytics_beijing_snapshot(record_time);
CREATE INDEX idx_beijing_bms_time ON analytics_beijing_bms(record_time);
CREATE INDEX idx_beijing_bms_temperature ON analytics_beijing_bms(max_temperature_c);
"""


def member_name(zf: zipfile.ZipFile, suffix: str) -> str:
    matches = [name for name in zf.namelist() if name.endswith(suffix)]
    if len(matches) != 1:
        raise RuntimeError(f"压缩包中未找到唯一文件 {suffix}: {matches}")
    return matches[0]


def as_int(value: str) -> int:
    return int(float(value))


def as_float(value: str) -> float:
    return float(value)


def create_backup(db_path: Path) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_path = db_path.with_name(f"{db_path.stem}-before-beijing-import-{timestamp}{db_path.suffix}")
    source = sqlite3.connect(str(db_path))
    target = sqlite3.connect(str(backup_path))
    try:
        source.backup(target)
    finally:
        target.close()
        source.close()
    return backup_path


def insert_stream(conn: sqlite3.Connection, zf: zipfile.ZipFile, *, suffix: str,
                  table: str, sql: str, transform, batch_size: int = 20_000) -> int:
    source_member = member_name(zf, suffix)
    started = time.monotonic()
    count = 0
    batch = []
    conn.execute("BEGIN IMMEDIATE")
    try:
        with zf.open(source_member, "r") as raw:
            with io.TextIOWrapper(raw, encoding="utf-8-sig", newline="") as text:
                for row in csv.DictReader(text):
                    batch.append(transform(row))
                    if len(batch) >= batch_size:
                        conn.executemany(sql, batch)
                        count += len(batch)
                        batch.clear()
                        if count % 200_000 == 0:
                            print(f"  {table}: {count:,} 行", flush=True)
                if batch:
                    conn.executemany(sql, batch)
                    count += len(batch)
        elapsed = time.monotonic() - started
        conn.execute(
            """INSERT OR REPLACE INTO analytics_beijing_import_log
               (table_name, source_member, row_count, imported_at, elapsed_seconds, status)
               VALUES (?, ?, ?, ?, ?, 'complete')""",
            (table, source_member, count, datetime.now().astimezone().isoformat(timespec="seconds"), elapsed),
        )
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    conn.execute("PRAGMA wal_checkpoint(PASSIVE)")
    print(f"  {table}: 完成，共 {count:,} 行，用时 {elapsed:.1f} 秒", flush=True)
    return count


def import_dataset(zip_path: Path, db_path: Path, make_backup: bool = True):
    if not zip_path.exists():
        raise FileNotFoundError(zip_path)

    db_path.parent.mkdir(parents=True, exist_ok=True)
    backup_path = create_backup(db_path) if make_backup and db_path.exists() else None
    if backup_path:
        print(f"已备份数据库: {backup_path}", flush=True)
    else:
        print(f"将创建数据库: {db_path}", flush=True)

    conn = sqlite3.connect(str(db_path), timeout=60)
    try:
        conn.execute("PRAGMA busy_timeout=60000")
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute("PRAGMA synchronous=NORMAL")
        conn.execute("PRAGMA temp_store=MEMORY")
        conn.execute("PRAGMA cache_size=-131072")
        conn.execute("PRAGMA mmap_size=268435456")
        conn.execute("PRAGMA foreign_keys=OFF")

        conn.execute("BEGIN IMMEDIATE")
        try:
            for table in TABLES:
                conn.execute(f"DROP TABLE IF EXISTS {table}")
            conn.execute("DROP TABLE IF EXISTS analytics_beijing_import_log")
            conn.execute("DROP TABLE IF EXISTS analytics_beijing_dataset_meta")
            conn.executescript(SCHEMA_SQL)
            conn.commit()
        except Exception:
            conn.rollback()
            raise

        counts = {}
        with zipfile.ZipFile(zip_path) as zf:
            counts["analytics_beijing_station"] = insert_stream(
                conn, zf, suffix="beijing_stations.csv", table="analytics_beijing_station",
                sql="INSERT INTO analytics_beijing_station VALUES (?,?,?,?,?,?,?,?)",
                transform=lambda r: (
                    as_int(r["stationId"]), r["locationId"], as_int(r["facilityType"]),
                    r["station_name"], r["address"], as_int(r["device_count"]),
                    r["open_time"], r["update_time"],
                ),
            )
            counts["analytics_beijing_weather"] = insert_stream(
                conn, zf, suffix="beijing_weather.csv", table="analytics_beijing_weather",
                sql="INSERT INTO analytics_beijing_weather VALUES (?,?,?,?,?,?,?)",
                transform=lambda r: (
                    r["date"], as_int(r["weekday"]), as_int(r["holiday"]),
                    as_float(r["temp_high"]), as_float(r["temp_low"]),
                    as_int(r["condition"]), as_float(r["precipitation"]),
                ),
            )
            counts["analytics_beijing_session"] = insert_stream(
                conn, zf, suffix="beijing_sessions.csv", table="analytics_beijing_session",
                sql="INSERT INTO analytics_beijing_session VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                transform=lambda r: (
                    as_int(r["sessionId"]), as_float(r["kwhTotal"]), as_float(r["charging_fees"]),
                    r["created"], r["ended"], as_int(r["startTime"]), as_int(r["endTime"]),
                    as_float(r["chargeTimeHrs"]), r["weekday"], r["platform"], as_int(r["userId"]),
                    as_int(r["stationId"]), r["locationId"], as_int(r["managerVehicle"]),
                    as_int(r["facilityType"]), as_int(r["Mon"]), as_int(r["Tues"]),
                    as_int(r["Wed"]), as_int(r["Thurs"]), as_int(r["Fri"]),
                    as_int(r["Sat"]), as_int(r["Sun"]),
                ),
            )
            counts["analytics_beijing_snapshot"] = insert_stream(
                conn, zf, suffix="beijing_snapshots.csv", table="analytics_beijing_snapshot",
                sql="INSERT INTO analytics_beijing_snapshot VALUES (?,?,?,?,?,?,?,?)",
                transform=lambda r: (
                    as_int(r["stationId"]), r["record_time"], as_int(r["in_use"]),
                    as_int(r["idle"]), as_int(r["fault"]), as_float(r["charging_power_kw"]),
                    as_int(r["queue_count"]), as_float(r["cum_kwh_day"]),
                ),
            )
            counts["analytics_beijing_bms"] = insert_stream(
                conn, zf, suffix="beijing_bms.csv", table="analytics_beijing_bms",
                sql="INSERT INTO analytics_beijing_bms VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                transform=lambda r: (
                    as_int(r["esd"]), r["record_time"], as_float(r["soc"]),
                    as_float(r["pack_voltage (V)"]), as_float(r["charge_current (A)"]),
                    as_float(r["max_cell_voltage (V)"]), as_float(r["min_cell_voltage (V)"]),
                    as_float(r["max_temperature (℃)"]), as_float(r["min_temperature (℃)"]),
                    as_float(r["available_energy (kw)"]), as_float(r["available_capacity (Ah)"]),
                ),
            )

        print("创建分析索引...", flush=True)
        conn.executescript(INDEX_SQL)
        imported_at = datetime.now().astimezone().isoformat(timespec="seconds")
        min_date, max_date = conn.execute(
            "SELECT MIN(substr(created,1,10)), MAX(substr(created,1,10)) FROM analytics_beijing_session"
        ).fetchone()
        conn.execute(
            """INSERT OR REPLACE INTO analytics_beijing_dataset_meta
               (dataset_name, source_zip, data_nature, min_date, max_date, imported_at, notes)
               VALUES (?, ?, ?, ?, ?, ?, ?)""",
            (
                "北京充电桩模拟数据集", zip_path.name, "模拟数据", min_date, max_date,
                imported_at, "完整导入5张CSV；analytics_beijing_* 与原业务表隔离",
            ),
        )
        conn.commit()

        # 恢复项目运行时设置，然后做关系与完整性校验。
        conn.execute("PRAGMA foreign_keys=ON")
        foreign_key_issues = list(conn.execute("PRAGMA foreign_key_check"))
        quick_check = conn.execute("PRAGMA quick_check").fetchone()[0]
        session_orphans = conn.execute(
            """SELECT COUNT(*) FROM analytics_beijing_session s
               LEFT JOIN analytics_beijing_station st ON st.station_id=s.station_id
               WHERE st.station_id IS NULL"""
        ).fetchone()[0]
        bms_orphans = conn.execute(
            """SELECT COUNT(*) FROM analytics_beijing_bms b
               LEFT JOIN analytics_beijing_session s ON s.session_id=b.esd
               WHERE s.session_id IS NULL"""
        ).fetchone()[0]
        conn.execute("PRAGMA optimize")
        conn.execute("PRAGMA wal_checkpoint(TRUNCATE)")

        print("导入校验:", flush=True)
        for table, count in counts.items():
            actual = conn.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
            print(f"  {table}: 源 {count:,} / 库 {actual:,}", flush=True)
            if actual != count:
                raise RuntimeError(f"{table} 行数不一致")
        print(f"  数据范围: {min_date} 至 {max_date}", flush=True)
        print(f"  会话孤儿记录: {session_orphans}", flush=True)
        print(f"  BMS孤儿记录: {bms_orphans}", flush=True)
        print(f"  外键问题: {len(foreign_key_issues)}", flush=True)
        print(f"  SQLite quick_check: {quick_check}", flush=True)
        if session_orphans or bms_orphans or foreign_key_issues or quick_check != "ok":
            raise RuntimeError("导入后数据完整性校验失败")
        return backup_path, counts
    finally:
        conn.close()


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="导入北京模拟充电数据集到 ChargingPlatform SQLite")
    parser.add_argument("--zip", dest="zip_path", type=Path, default=root / "05.北京模拟充电数据集.zip")
    parser.add_argument("--db", dest="db_path", type=Path, default=root / "database" / "test.db")
    parser.add_argument("--no-backup", action="store_true", help="跳过导入前数据库备份")
    args = parser.parse_args()
    started = time.monotonic()
    try:
        backup_path, counts = import_dataset(
            args.zip_path.resolve(), args.db_path.resolve(), make_backup=not args.no_backup
        )
    except Exception as exc:
        print(f"导入失败: {exc}", file=sys.stderr, flush=True)
        raise
    elapsed = time.monotonic() - started
    print(f"导入完成: {sum(counts.values()):,} 行，总用时 {elapsed:.1f} 秒", flush=True)
    print(f"数据库: {args.db_path.resolve()}", flush=True)
    if backup_path:
        print(f"可恢复备份: {backup_path}", flush=True)


if __name__ == "__main__":
    main()
