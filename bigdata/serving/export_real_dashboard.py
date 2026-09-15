"""Publish the real Boulder experiment as the active seven-file Web bundle."""
from __future__ import annotations

import argparse
import datetime as dt
import gzip
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from zoneinfo import ZoneInfo

import numpy as np


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_json import publish  # noqa: E402


ZONE = ZoneInfo("America/Denver")
HORIZONS = (1, 6, 24)
MODEL_NAME = {"spark_gbt": "shared_gbt", "residual_cnn": "residual_cnn", "8week": "8week"}


def wall_time(epoch: int) -> dt.datetime:
    # event_time was exported from Spark source-local wall-clock timestamps.
    return dt.datetime.fromtimestamp(int(epoch)).replace(tzinfo=ZONE)


def iso(value: dt.datetime) -> str:
    return value.isoformat(timespec="seconds")


def seasonal_profile(values: np.ndarray, horizon: int = 24) -> np.ndarray:
    end = len(values)
    result = []
    for offset in range(horizon):
        history = [values[end + offset - 168 * week] for week in range(1, 9)
                   if 0 <= end + offset - 168 * week < end]
        result.append(float(np.mean(history)) if history else 0.0)
    return np.asarray(result, dtype=np.float64)


def clean_metric(value: dict) -> dict:
    return {key: value.get(key) for key in ("n", "mae", "rmse", "wmape", "smape", "r2")}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", default="real_boulder_20260915_v1")
    parser.add_argument("--npz", default=str(REPO / ".bigdata/experiments/real_boulder_load.npz"))
    parser.add_argument("--spark-report", default=str(REPO / "bigdata/reports/real_boulder_spark_run.json"))
    parser.add_argument("--cnn-report", default=str(REPO / "bigdata/reports/real_boulder_cnn_experiment.json"))
    args = parser.parse_args()

    raw = np.load(Path(args.npz))
    spark_report = json.loads(Path(args.spark_report).read_text(encoding="utf-8"))
    cnn_report = json.loads(Path(args.cnn_report).read_text(encoding="utf-8"))
    manifest = json.loads((REPO / ".bigdata/source/real/boulder/manifest.json").read_text())
    station_meta = {int(row["station_id"]): row for row in spark_report["selected_stations"]}
    name_to_id = {row["station_name"]: station_id for station_id, row in station_meta.items()}

    addresses: dict[int, str] = {}
    orders_by_day_station: Counter[tuple[dt.date, int]] = Counter()
    for page in sorted((REPO / ".bigdata/source/real/boulder/pages").glob("*.jsonl.gz")):
        with gzip.open(page, "rt", encoding="utf-8") as stream:
            for line in stream:
                row = json.loads(line)
                station_id = name_to_id.get((row.get("Station_Name") or "").strip())
                if station_id is None:
                    continue
                addresses.setdefault(station_id, row.get("Address") or "Boulder, Colorado")
                try:
                    start = dt.datetime.strptime(row["Start_Date___Time"], "%m/%d/%Y %H:%M")
                    end = dt.datetime.strptime(row["End_Date___Time"], "%m/%d/%Y %H:%M")
                    energy = float(row["Energy__kWh_"])
                except (KeyError, TypeError, ValueError):
                    continue
                if end > start and 0 < energy <= 300 and (end - start) <= dt.timedelta(hours=72):
                    orders_by_day_station[(start.date(), station_id)] += 1

    station_series = {}
    all_times = []
    for station_id in sorted(station_meta):
        mask = raw["station_id"] == station_id
        times = np.asarray([wall_time(value) for value in raw["event_time"][mask]])
        values = raw["load_kwh"][mask].astype(np.float64)
        station_series[station_id] = (mask, times, values)
        all_times.extend(times.tolist())
    data_time = max(all_times)
    operation_date = data_time.date() - dt.timedelta(days=1)

    daily = defaultdict(float)
    station_daily = defaultdict(float)
    hour_band = defaultdict(float)
    weekday_energy = defaultdict(float)
    weekday_hours = Counter()
    for station_id, (_, times, values) in station_series.items():
        for timestamp, value in zip(times, values):
            day = timestamp.date()
            daily[day] += float(value)
            station_daily[(station_id, day)] += float(value)
            band = "夜间(00–06)" if timestamp.hour < 6 else (
                "日间(06–18)" if timestamp.hour < 18 else "晚间(18–24)")
            hour_band[band] += float(value)
            kind = 1 if timestamp.weekday() >= 5 else 0
            weekday_energy[kind] += float(value)
            weekday_hours[kind] += 1

    forecasts = []
    rankings = []
    for station_id, meta in station_meta.items():
        mask, times, values = station_series[station_id]
        origin = times[-1] + dt.timedelta(hours=1)
        h24_selection = cnn_report["station_selection"]["24"]["stations"][str(station_id)]
        profile = seasonal_profile(values)
        target_total = float(h24_selection["prediction_kwh"])
        profile = profile * target_total / profile.sum() if profile.sum() > 0 else np.full(24, target_total / 24)
        points = [{
            "horizon": step + 1,
            "time": iso(origin + dt.timedelta(hours=step)),
            "load_kwh": float(max(0, value)),
            "lower_kwh": float(max(0, value)),
            "upper_kwh": float(max(0, value)),
            "idle": None, "queue": None, "capacity_kwh": None, "alert": "normal",
        } for step, value in enumerate(profile)]
        direct = {}
        for horizon in HORIZONS:
            selected = cnn_report["station_selection"][str(horizon)]["stations"][str(station_id)]
            winner = selected["winner"]
            chosen = clean_metric(selected["test"][winner])
            baseline = clean_metric(selected["test"]["8week"])
            improvement = ((baseline["mae"] - chosen["mae"]) / baseline["mae"]
                           if baseline["mae"] else None)
            direct[str(horizon)] = {
                "model": MODEL_NAME[winner],
                "prediction_kwh": float(selected["prediction_kwh"]),
                "metrics": chosen,
                "baseline_metrics": baseline,
                "improvement": improvement,
                "selection": "按验证集 MAE 在残差 CNN、Spark GBT、8 周基线中逐站选优",
                "candidate_version": cnn_report["run_id"],
                "candidate_predictions_kwh": {
                    MODEL_NAME[name]: float(value)
                    for name, value in selected["candidate_predictions_kwh"].items()
                },
            }
        baseline_test = clean_metric(h24_selection["test"]["8week"])
        recent_indexes = np.where(raw["split"][mask] == 2)[0][-168:]
        recent = [{"time": iso(times[index]), "actual_kwh": float(values[index]),
                   "predicted_kwh": float(max(0, raw["gbt_h1"][mask][index]))}
                  for index in recent_indexes]
        forecasts.append({
            "station_id": station_id, "name": meta["station_name"], "district": "Boulder, CO",
            "device_count": 1, "model": "8week", "model_version": cnn_report["run_id"],
            "origin": iso(origin - dt.timedelta(hours=1)),
            "train_start": str(meta["first_session"]),
            "train_end": str((times[-1] - dt.timedelta(days=120)).date()),
            "metrics": baseline_test, "baseline_metrics": baseline_test, "baseline": "8week",
            "improvement": 0.0, "needs_optimization": h24_selection["winner"] == "8week",
            "test_degraded": (h24_selection["test"][h24_selection["winner"]]["mae"] >
                              h24_selection["test"]["8week"]["mae"]),
            "feature_importance": {}, "rolling_backtests": [], "recent_backtest": recent,
            "direct_horizons": direct,
            "interval_method": "真实源未提供功率采样，暂不输出伪造置信区间",
            "exogenous": "真实会话电量、历史负荷、小时/星期周期、1/2/24/48/168 小时滞后",
            "history": [{"time": iso(timestamp), "load_kwh": float(value)}
                        for timestamp, value in zip(times[-24:], values[-24:])],
            "points": points,
        })
        score = max(0.0, 100 * (1 - h24_selection["test"][h24_selection["winner"]]["wmape"]))
        rankings.append({
            "station_id": station_id, "station_name": meta["station_name"],
            "district": "Boulder, CO", "device_count": 1,
            "energy_kwh": station_daily[(station_id, operation_date)],
            "orders": orders_by_day_station[(operation_date, station_id)],
            "revenue": None, "utilization": None, "fault_rate": None, "peak_queue": None,
            "health": score, "run_id": args.run_id, "data_nature": "真实数据",
            "data_time": iso(data_time),
        })
    rankings.sort(key=lambda row: (-row["health"], row["station_id"]))

    operation_energy = sum(station_daily[(station_id, operation_date)] for station_id in station_meta)
    operation_orders = sum(orders_by_day_station[(operation_date, station_id)] for station_id in station_meta)
    overview = {
        "operation_overview": [{
            "energy_kwh": operation_energy, "revenue": None, "orders": operation_orders,
            "utilization": None, "fault_rate": None, "total_users": None, "active_users": None,
            "device_count": len(station_meta), "run_id": args.run_id,
            "data_nature": "真实数据", "data_time": iso(data_time),
        }],
        "station_health_rank": rankings,
        "district_map": [{"district": "Boulder, CO", "energy_kwh": operation_energy}],
        "operation_trend": [{"dt": day.isoformat(), "energy_kwh": daily[day]}
                            for day in sorted(daily)[-30:]],
        "load_factors": [{"holiday": kind,
                          "avg_kwh": weekday_energy[kind] / max(1, weekday_hours[kind]),
                          "hours": weekday_hours[kind]} for kind in (0, 1)],
        "tariff_contribution": [{"band": band, "energy_kwh": hour_band[band]}
                                for band in ("夜间(00–06)", "日间(06–18)", "晚间(18–24)")],
        "realtime_alert": [],
        "user_segments": [{"segment": "源数据无用户标识", "users": 0, "avg_kwh": 0}],
        "load_discrepancy": [{"comparable": False,
                              "reason": "源数据无独立设备遥测累计电量"}],
        "device_status": [{"idle": 0, "busy": 0, "fault": 0,
                           "available": False, "reason": "源数据无设备状态快照"}],
    }

    empty_time = {"n": 0, "hit_rate_1": None, "hit_rate_3": None, "mrr": None, "ndcg_3": None}
    empty_energy = {"n": 0, "mae": None, "rmse": None, "wmape": None, "smape": None, "r2": None}
    user = {
        "version": args.run_id, "feature_version": spark_report["run_id"],
        "application_id": spark_report["application_id"],
        "time_model": "global_popular", "energy_model": "user_mean",
        "evaluation": {split: {"time": {"global_popular": empty_time},
                               "energy": {"user_mean": empty_energy}}
                       for split in ("validation", "test")},
        "user_count": 0, "coverage": 0.0, "cold_start_count": 0,
        "time_features": "源数据没有匿名用户标识，不能进行用户级预测",
        "heatmap": [[hour, day, 0] for day in range(7) for hour in range(24)],
        "next_slot_distribution": [0] * 168,
        "next_day_distribution": {(operation_date + dt.timedelta(days=i)).isoformat(): 0
                                  for i in range(1, 8)},
        "energy_distribution": [{"from": 0, "to": 0, "count": 0}],
        "mean_next_kwh": 0, "fallback": "不可用：官方数据不含用户 ID",
        "cold_start_example": {"personalized": False, "next_kwh": 0,
                               "basis": "源数据不支持用户预测"},
        "origin": iso(data_time),
    }

    reasons = {
        "invalid_timestamp": "起止时间无法解析",
        "invalid_duration": "结束早于开始或总时长超过 72 小时",
        "invalid_energy": "电量缺失、非正数或超过 300 kWh",
        "invalid_charging_time": "充电时长缺失或与总时长冲突",
    }
    quality = [{
        "run_id": args.run_id, "table_name": "charging_session", "column_name": "*",
        "rule_id": rule, "rule_description": reasons[rule], "severity": "ERROR",
        "total_count": spark_report["raw_records"], "failed_count": int(count),
        "failed_rate": count / spark_report["raw_records"], "sample_keys": [],
        "action": "reject_to_hdfs_quarantine", "detected_at": spark_report["created_at"],
    } for rule, count in spark_report["quality_reasons"].items()]
    quality.append({
        "run_id": args.run_id, "table_name": "charging_session", "column_name": "ObjectId2",
        "rule_id": "duplicate", "rule_description": "交易主键重复", "severity": "ERROR",
        "total_count": spark_report["raw_records"],
        "failed_count": spark_report["raw_records"] - spark_report["deduplicated_records"],
        "failed_rate": 0.0, "sample_keys": [], "action": "reject_to_hdfs_quarantine",
        "detected_at": spark_report["created_at"],
    })

    winner_counts = {h: cnn_report["station_selection"][str(h)]["winner_counts"] for h in HORIZONS}
    health = {
        "storage": "hdfs://localhost:9000/charging_real/boulder", "master": "yarn",
        "stages": [
            {"run_id": args.run_id, "stage": "download_official_api", "status": "SUCCESS",
             "application_id": "external-http"},
            {"run_id": args.run_id, "stage": "pyspark_clean_sparksql_gbt", "status": "SUCCESS",
             "application_id": spark_report["application_id"]},
            {"run_id": args.run_id, "stage": "residual_cnn", "status": "SUCCESS",
             "application_id": "pytorch-local-cpu"},
        ],
        "layers": {layer: f"/charging_real/boulder/{path}" for layer, path in {
            "ods": "source-manifest", "dwd": "dwd/sessions", "dws": "features/hourly",
            "features": "features/hourly/version=" + spark_report["run_id"],
            "models": "models/residual_cnn/version=" + cnn_report["run_id"],
            "ads": "code/web/data/runs",
        }.items()},
        "quality_rule_count": len(quality), "warning_rows": 0,
        "error_rule_hits": spark_report["rejected_records"], "status": "SUCCESS",
        "conservation": {"status": "PASS_BY_CONSTRUCTION",
                         "method": "按会话充电秒数分摊，小时份额和为会话总电量"},
        "source_manifest": [{"name": manifest["dataset"], "rows": manifest["record_count"],
                             "sha256": manifest["ordered_pages_sha256"],
                             "license": manifest["license"], "url": manifest["api_url"]}],
        "model_count": len(forecasts),
        "baseline_fallbacks": sum(row["direct_horizons"]["24"]["model"] == "8week"
                                  for row in forecasts),
        "test_degraded_count": sum(row["test_degraded"] for row in forecasts),
        "winner_counts": winner_counts,
    }

    generated = dt.datetime.now().astimezone().isoformat(timespec="seconds")
    raw_payloads = {
        "overview": overview,
        "stations": [{"station_id": station_id, "name": meta["station_name"],
                      "district": "Boulder, CO", "device_count": 1,
                      "address": addresses.get(station_id)}
                     for station_id, meta in station_meta.items()],
        "station-ranking": rankings, "load-forecast": forecasts,
        "user-demand": user, "data-quality": quality, "pipeline-health": health,
    }
    payloads = {name: {"schemaVersion": "1.0", "dataNature": "真实数据",
                       "dataTime": iso(data_time), "generatedAt": generated,
                       "runId": args.run_id, "data": value}
                for name, value in raw_payloads.items()}
    envelope = json.loads((REPO / "bigdata/serving/schemas/envelope.schema.json").read_text())
    generation = publish(REPO / "code/web/data", payloads, envelope, args.run_id)
    print(json.dumps({"generation": generation, "run_id": args.run_id,
                      "data_time": iso(data_time), "operation_date": operation_date.isoformat(),
                      "energy_kwh": operation_energy, "orders": operation_orders,
                      "stations": len(forecasts)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
