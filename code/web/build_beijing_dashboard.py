#!/usr/bin/env python3
"""从北京模拟充电数据集生成 Web 大屏静态数据。

只读取 zip，不修改原始数据。输出 dashboard-data.json，可由大屏直接加载。
当前版本使用历史同星期均值作为可复现的预测基线；后续 Spark ML 模型可按相同
JSON 契约替换 forecast 字段。
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import sqlite3
import statistics
import zipfile
from collections import defaultdict
from datetime import date, datetime, timedelta
from pathlib import Path


DISTRICT_CENTERS = {
    "东城区": (116.4219, 39.9386),
    "西城区": (116.3659, 39.9123),
    "朝阳区": (116.4864, 39.9215),
    "海淀区": (116.2981, 39.9593),
    "丰台区": (116.2869, 39.8585),
    "石景山区": (116.2229, 39.9066),
    "通州区": (116.6574, 39.9097),
    "顺义区": (116.6546, 40.1302),
    "大兴区": (116.3414, 39.7269),
    "昌平区": (116.2312, 40.2207),
    "房山区": (116.1433, 39.7488),
    "门头沟区": (116.1017, 39.9404),
    "怀柔区": (116.6317, 40.3160),
    "密云区": (116.8434, 40.3763),
    "延庆区": (115.9749, 40.4568),
    "平谷区": (117.1214, 40.1406),
}


def archive_member(zf: zipfile.ZipFile, suffix: str) -> str:
    matches = [name for name in zf.namelist() if name.endswith(suffix)]
    if len(matches) != 1:
        raise RuntimeError(f"压缩包中未找到唯一文件 {suffix}: {matches}")
    return matches[0]


def csv_rows(zf: zipfile.ZipFile, suffix: str):
    import io

    with zf.open(archive_member(zf, suffix), "r") as raw:
        with io.TextIOWrapper(raw, encoding="utf-8-sig", newline="") as text:
            yield from csv.DictReader(text)


def safe_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def safe_int(value, default=0):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def percent_change(current: float, previous: float):
    if previous == 0:
        return None
    return round((current - previous) / previous * 100, 1)


def station_coordinate(district: str, station_id: int):
    """数据集无经纬度，使用区中心加稳定偏移生成地图示意坐标。"""
    center_lon, center_lat = DISTRICT_CENTERS.get(district, (116.4074, 39.9042))
    ordinal = max(station_id - 1001, 0)
    angle = math.radians((ordinal * 137.508) % 360)
    ring = 0.012 + (ordinal % 4) * 0.007
    lon = center_lon + math.cos(angle) * ring
    lat = center_lat + math.sin(angle) * ring * 0.72
    return round(lon, 6), round(lat, 6)


def mean_hourly(day_values, candidate_days):
    result = []
    for hour in range(24):
        values = [day_values[d][hour] for d in candidate_days if day_values[d][hour] is not None]
        result.append(round(statistics.fmean(values), 2) if values else 0.0)
    return result


def load_navigation_stations(db_path: Path):
    if not db_path.exists():
        return []
    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row
    try:
        try:
            rows = conn.execute(
                """
                SELECT s.id, s.name, s.address, s.longitude, s.latitude, s.price,
                       COUNT(p.id) AS total_piles,
                       SUM(CASE WHEN p.status = 0 THEN 1 ELSE 0 END) AS idle_piles
                FROM station s LEFT JOIN pile p ON p.station_id = s.id
                GROUP BY s.id ORDER BY s.id
                """
            ).fetchall()
        except sqlite3.Error:
            # 新成员可能先导入分析库、后启动 Qt 服务端；此时业务站点表尚未创建。
            # 大屏会自动使用数据集的行政区示意坐标，不让首次构建失败。
            return []
        return [
            {
                "id": row["id"],
                "name": row["name"],
                "address": row["address"],
                "lon": row["longitude"],
                "lat": row["latitude"],
                "price": row["price"],
                "totalPiles": row["total_piles"],
                "idlePiles": row["idle_piles"] or 0,
                "distance": 0,
                "level": "normal",
            }
            for row in rows
            if row["longitude"] and row["latitude"]
        ]
    finally:
        conn.close()


def build(zip_path: Path, db_path: Path):
    with zipfile.ZipFile(zip_path) as zf:
        stations = {}
        for row in csv_rows(zf, "beijing_stations.csv"):
            station_id = safe_int(row["stationId"])
            lon, lat = station_coordinate(row["locationId"], station_id)
            stations[station_id] = {
                "stationId": station_id,
                "district": row["locationId"],
                "facilityType": safe_int(row["facilityType"]),
                "name": row["station_name"],
                "address": row["address"],
                "deviceCount": safe_int(row["device_count"]),
                "lon": lon,
                "lat": lat,
            }

        daily_sessions = defaultdict(lambda: {"revenue": 0.0, "energy": 0.0, "orders": 0})
        daily_station = defaultdict(lambda: defaultdict(lambda: {"revenue": 0.0, "energy": 0.0, "orders": 0}))
        users = set()
        daily_users = defaultdict(set)

        for row in csv_rows(zf, "beijing_sessions.csv"):
            day = row["created"][:10]
            station_id = safe_int(row["stationId"])
            revenue = safe_float(row["charging_fees"])
            energy = safe_float(row["kwhTotal"])
            user_id = row["userId"]
            daily_sessions[day]["revenue"] += revenue
            daily_sessions[day]["energy"] += energy
            daily_sessions[day]["orders"] += 1
            daily_station[day][station_id]["revenue"] += revenue
            daily_station[day][station_id]["energy"] += energy
            daily_station[day][station_id]["orders"] += 1
            users.add(user_id)
            daily_users[day].add(user_id)

        hourly_sum = defaultdict(lambda: [0.0] * 24)
        hourly_count = defaultdict(lambda: [0] * 24)
        latest_by_station = {}
        daily_station_snap = defaultdict(lambda: defaultdict(lambda: {
            "inUseSum": 0,
            "idleSum": 0,
            "faultSum": 0,
            "queueSum": 0,
            "powerSum": 0.0,
            "samples": 0,
            "maxFault": 0,
            "maxFaultTime": "",
            "maxQueue": 0,
            "maxQueueTime": "",
            "lastTime": "",
            "last": None,
        }))

        for row in csv_rows(zf, "beijing_snapshots.csv"):
            station_id = safe_int(row["stationId"])
            timestamp = row["record_time"]
            day = timestamp[:10]
            hour = safe_int(timestamp[11:13])
            power = safe_float(row["charging_power_kw"])
            hourly_sum[day][hour] += power
            hourly_count[day][hour] += 1
            snap = {
                "time": timestamp,
                "inUse": safe_int(row["in_use"]),
                "idle": safe_int(row["idle"]),
                "fault": safe_int(row["fault"]),
                "power": power,
                "queue": safe_int(row["queue_count"]),
                "cumKwh": safe_float(row["cum_kwh_day"]),
            }
            if station_id not in latest_by_station or timestamp > latest_by_station[station_id]["time"]:
                latest_by_station[station_id] = snap

            agg = daily_station_snap[day][station_id]
            agg["inUseSum"] += snap["inUse"]
            agg["idleSum"] += snap["idle"]
            agg["faultSum"] += snap["fault"]
            agg["queueSum"] += snap["queue"]
            agg["powerSum"] += snap["power"]
            agg["samples"] += 1
            if snap["fault"] > agg["maxFault"]:
                agg["maxFault"] = snap["fault"]
                agg["maxFaultTime"] = timestamp
            if snap["queue"] > agg["maxQueue"]:
                agg["maxQueue"] = snap["queue"]
                agg["maxQueueTime"] = timestamp
            if timestamp > agg["lastTime"]:
                agg["lastTime"] = timestamp
                agg["last"] = snap

    all_days = sorted(set(daily_sessions) & set(hourly_sum))
    if not all_days:
        raise RuntimeError("没有可用于计算的会话与快照日期")
    latest_day = all_days[-1]
    previous_day = all_days[-2]
    latest_date = date.fromisoformat(latest_day)

    day_hourly = {}
    for day in all_days:
        day_hourly[day] = [
            round(hourly_sum[day][hour] / hourly_count[day][hour], 2)
            if hourly_count[day][hour]
            else None
            for hour in range(24)
        ]

    # 次日基线：最近 8 个相同星期的全网站点平均负荷。
    forecast_day = latest_date + timedelta(days=1)
    candidates = [
        day for day in all_days[:-1]
        if date.fromisoformat(day).weekday() == forecast_day.weekday()
    ][-8:]
    forecast = mean_hourly(day_hourly, candidates)

    # 滚动回测最近 14 天，每天只使用此前同星期数据。
    errors = []
    for target_day in all_days[-14:]:
        target_date = date.fromisoformat(target_day)
        history = [
            day for day in all_days
            if day < target_day and date.fromisoformat(day).weekday() == target_date.weekday()
        ][-8:]
        if not history:
            continue
        predicted = mean_hourly(day_hourly, history)
        for actual, estimate in zip(day_hourly[target_day], predicted):
            if actual is not None and actual > 1:
                errors.append(abs(actual - estimate) / actual)
    mape = round(statistics.fmean(errors) * 100, 1) if errors else None
    band_ratio = min(max((mape or 12.0) / 100 * 1.28, 0.10), 0.28)

    all_network_powers = [value for day in day_hourly.values() for value in day if value is not None]
    sorted_powers = sorted(all_network_powers)
    p95 = sorted_powers[min(int(len(sorted_powers) * 0.95), len(sorted_powers) - 1)]
    threshold = round(p95, 2)
    peak_hour = max(range(24), key=lambda hour: forecast[hour])

    current = daily_sessions[latest_day]
    previous = daily_sessions[previous_day]
    current_snaps = daily_station_snap[latest_day]
    previous_snaps = daily_station_snap[previous_day]

    total_devices = sum(station["deviceCount"] for station in stations.values())
    current_last = [agg["last"] for agg in current_snaps.values() if agg["last"]]
    prev_last = [agg["last"] for agg in previous_snaps.values() if agg["last"]]
    pile_state = {
        "inUse": sum(item["inUse"] for item in current_last),
        "idle": sum(item["idle"] for item in current_last),
        "fault": sum(item["fault"] for item in current_last),
    }
    previous_online = sum(item["inUse"] + item["idle"] for item in prev_last)
    online = pile_state["inUse"] + pile_state["idle"]
    avg_in_use = sum(agg["inUseSum"] for agg in current_snaps.values())
    avg_capacity = sum(
        agg["inUseSum"] + agg["idleSum"] + agg["faultSum"] for agg in current_snaps.values()
    )
    utilization = avg_in_use / avg_capacity * 100 if avg_capacity else 0
    fault_snapshot_sum = sum(agg["faultSum"] for agg in current_snaps.values())
    fault_rate = fault_snapshot_sum / avg_capacity * 100 if avg_capacity else 0

    station_rows = []
    for station_id, station in stations.items():
        agg = current_snaps.get(station_id)
        last = agg["last"] if agg else None
        total = station["deviceCount"]
        in_use = last["inUse"] if last else 0
        idle = last["idle"] if last else 0
        fault = last["fault"] if last else 0
        samples = agg["samples"] if agg else 0
        avg_queue = agg["queueSum"] / samples if samples else 0
        avg_use_rate = agg["inUseSum"] / max(total * samples, 1) if samples else 0
        day_fault_rate = agg["faultSum"] / max(total * samples, 1) if samples else 0
        max_fault = agg["maxFault"] if agg else 0
        max_queue = agg["maxQueue"] if agg else 0
        health = 100 - day_fault_rate * 200 - (max_fault / max(total, 1) * 30) - min(avg_queue * 2.5, 15)
        if avg_use_rate > 0.85:
            health -= (avg_use_rate - 0.85) * 50
        health = max(0, min(100, round(health)))
        level = "fault" if max_fault else ("busy" if avg_use_rate >= 0.7 or max_queue >= 1 else "normal")
        station_rows.append({
            **station,
            "health": health,
            "level": level,
            "online": in_use + idle,
            "idle": idle,
            "inUse": in_use,
            "fault": fault,
            "queue": last["queue"] if last else 0,
            "maxFault": max_fault,
            "maxFaultTime": agg["maxFaultTime"] if agg else "",
            "maxQueue": max_queue,
            "maxQueueTime": agg["maxQueueTime"] if agg else "",
            "utilization": round(avg_use_rate * 100, 1),
            "revenue": round(daily_station[latest_day][station_id]["revenue"], 2),
            "energy": round(daily_station[latest_day][station_id]["energy"], 2),
        })

    station_rows.sort(key=lambda item: (-item["health"], -item["utilization"], item["stationId"]))
    map_stations = sorted(station_rows, key=lambda item: item["stationId"])

    events = []
    fault_stations = sorted((s for s in station_rows if s["maxFault"]), key=lambda s: (-s["maxFault"], s["health"]))
    busy_stations = sorted((s for s in station_rows if s["maxQueue"]), key=lambda s: (-s["maxQueue"], -s["utilization"]))
    for station in fault_stations[:3]:
        events.append({"time": station["maxFaultTime"][11:16], "level": "urgent", "text": f"数据日检测到 {station['maxFault']} 个故障电桩", "station": station["name"]})
    for station in busy_stations[:4]:
        events.append({"time": station["maxQueueTime"][11:16], "level": "warning", "text": f"排队峰值 {station['maxQueue']} 辆，日均利用率 {station['utilization']:.1f}%", "station": station["name"]})
    events.extend([
        {"time": "数据日", "level": "info", "text": f"完成 {current['orders']:,} 笔充电会话", "station": "全网"},
        {"time": "预测", "level": "info", "text": f"次日高峰预计出现在 {peak_hour:02d}:00", "station": "全网"},
    ])

    available_at_peak = sum(s["idle"] for s in station_rows)
    peak_increase = percent_change(forecast[peak_hour], day_hourly[latest_day][peak_hour] or 0)
    advice = {
        "title": f"{peak_hour:02d}:00—{(peak_hour + 2) % 24:02d}:00 预计进入高峰",
        "action": "建议开放备用桩并提前安排运维值守",
        "reasons": [
            f"历史同星期平均负荷在 {peak_hour:02d}:00 达到 {forecast[peak_hour]:.1f} kW",
            f"相对最新数据日同小时变化 {peak_increase:+.1f}%" if peak_increase is not None else "最新数据日缺少可比负荷",
            f"当前空闲电桩 {available_at_peak} 个，故障电桩 {pile_state['fault']} 个",
            f"建议优先巡检 {fault_stations[0]['name']}" if fault_stations else "当前无故障桩，继续监测设备温度与通信状态",
        ],
    }

    navigation_stations = load_navigation_stations(db_path)
    output = {
        "meta": {
            "dataset": "北京充电桩模拟数据集",
            "dataNature": "模拟数据",
            "latestDate": latest_day,
            "previousDate": previous_day,
            "generatedAt": datetime.now().astimezone().isoformat(timespec="seconds"),
            "dataSource": "ZIP CSV → 本地聚合",
            "pipeline": ["HDFS", "Hive", "Spark"],
            "pipelineStatus": "规划接入",
            "modelName": "历史同星期基线",
            "modelVersion": "weekday-baseline-v1",
            "coordinateNote": "数据集无经纬度，地图点位按行政区中心生成示意坐标",
        },
        "kpis": {
            "revenue": {"value": round(current["revenue"], 2), "change": percent_change(current["revenue"], previous["revenue"])},
            "orders": {"value": current["orders"], "change": percent_change(current["orders"], previous["orders"])},
            "energy": {"value": round(current["energy"], 2), "change": percent_change(current["energy"], previous["energy"])},
            "online": {"value": online, "total": total_devices, "delta": online - previous_online},
            "utilization": {"value": round(utilization, 1)},
            "faultRate": {"value": round(fault_rate, 1)},
            "users": {"value": len(users), "today": len(daily_users[latest_day])},
        },
        "pileStatus": {**pile_state, "total": total_devices},
        "stationRanking": station_rows[:10],
        "mapStations": map_stations,
        "navigationStations": navigation_stations,
        "events": events[:8],
        "forecast": {
            "targetDate": forecast_day.isoformat(),
            "hours": [f"{hour:02d}:00" for hour in range(24)],
            "actualDate": latest_day,
            "actual": [value or 0 for value in day_hourly[latest_day]],
            "predicted": forecast,
            "lower": [round(max(0, value * (1 - band_ratio)), 2) for value in forecast],
            "upper": [round(value * (1 + band_ratio), 2) for value in forecast],
            "threshold": threshold,
            "thresholdLabel": "历史P95警戒线",
            "peakHour": peak_hour,
            "peakLoad": forecast[peak_hour],
            "mape": mape,
            "backtestDays": min(14, len(all_days)),
        },
        "advice": advice,
        "methodology": {
            "utilization": "数据日全部15分钟快照的在用桩数 ÷ 可用总桩时",
            "health": "100 - 故障扣分 - 排队扣分 - 高负荷扣分",
            "forecast": "最近8个相同星期的小时级全网站点平均负荷",
            "mape": "最近14天滚动回测，忽略实际负荷≤1kW的时点",
        },
    }
    return output


def main():
    script_dir = Path(__file__).resolve().parent
    default_zip = script_dir.parent.parent / "05.北京模拟充电数据集.zip"
    default_db = script_dir.parent.parent / "database" / "test.db"
    parser = argparse.ArgumentParser(description="生成北京模拟充电数据集大屏 JSON")
    parser.add_argument("--zip", dest="zip_path", type=Path, default=default_zip)
    parser.add_argument("--db", dest="db_path", type=Path, default=default_db)
    parser.add_argument("--output", type=Path, default=script_dir / "dashboard-data.json")
    args = parser.parse_args()

    payload = build(args.zip_path.resolve(), args.db_path.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"已生成 {args.output}")
    print(f"数据日期: {payload['meta']['latestDate']}")
    print(f"订单: {payload['kpis']['orders']['value']:,}")
    print(f"营收: {payload['kpis']['revenue']['value']:,.2f} 元")
    print(f"充电量: {payload['kpis']['energy']['value']:,.2f} kWh")
    print(f"预测 MAPE: {payload['forecast']['mape']}%")


if __name__ == "__main__":
    main()
