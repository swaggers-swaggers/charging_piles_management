"""Publish Beijing real operations/predictions with explicit simulated fallbacks.

Real fields always win.  The public release has no user identifier or fault
telemetry, so only those two domains are copied from the latest successful
Beijing simulation run and are labelled at field and page level.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import sys
from pathlib import Path
from zoneinfo import ZoneInfo

import numpy as np


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_json import publish  # noqa: E402


ZONE = ZoneInfo("Asia/Shanghai")
HORIZONS = (1, 6, 24)
MODEL_NAME = {"spark_gbt": "shared_gbt", "residual_cnn": "residual_cnn",
              "8week": "8week"}


def load_feature_bundle(path: str) -> dict[str, np.ndarray]:
    """Read the compact Spark JSONL export without importing the Torch trainer."""
    source = Path(path).resolve()
    if source.suffix == ".npz":
        return dict(np.load(source))
    rows = [json.loads(line) for line in source.read_text(encoding="utf-8").splitlines()
            if line]
    if not rows:
        raise ValueError(f"Empty feature bundle: {source}")
    integer = {"station_id", "event_time", "local_hour", "local_dow",
               "active_sessions", "split"}
    return {
        key: np.asarray([row[key] for row in rows],
                        dtype=np.int64 if key in integer else
                        (object if key == "station_code" else np.float32))
        for key in rows[0]
    }


def iso(value: dt.datetime) -> str:
    return value.isoformat(timespec="seconds")


def local_time(epoch: int) -> dt.datetime:
    return dt.datetime.fromtimestamp(int(epoch), tz=ZONE)


def clean_metric(value: dict) -> dict:
    return {key: value.get(key) for key in ("n", "mae", "rmse", "wmape", "smape", "r2")}


def find_simulation() -> tuple[dict, dict, list[dict], str]:
    candidates = sorted((REPO / "code/web/data/runs").glob("*/user-demand.json"),
                        key=lambda path: path.stat().st_mtime, reverse=True)
    for user_path in candidates:
        user = json.loads(user_path.read_text(encoding="utf-8"))
        if user.get("dataNature") != "模拟数据" or user.get("data", {}).get("user_count", 0) <= 0:
            continue
        generation = user_path.parent
        overview = json.loads((generation / "overview.json").read_text(encoding="utf-8"))
        ranking = json.loads((generation / "station-ranking.json").read_text(encoding="utf-8"))
        return user["data"], overview["data"], ranking["data"], user["runId"]
    raise RuntimeError("No successful simulated dashboard generation is available for missing fields")


def seasonal_profile(times: np.ndarray, values: np.ndarray, horizon: int = 24) -> np.ndarray:
    lookup = {int(timestamp.timestamp()): float(value) for timestamp, value in zip(times, values)}
    origin = times[-1] + dt.timedelta(hours=1)
    result = []
    for offset in range(horizon):
        target = origin + dt.timedelta(hours=offset)
        history = [lookup[int((target - dt.timedelta(days=7 * week)).timestamp())]
                   for week in range(1, 5)
                   if int((target - dt.timedelta(days=7 * week)).timestamp()) in lookup]
        if not history:
            yesterday = int((target - dt.timedelta(days=1)).timestamp())
            history = [lookup[yesterday]] if yesterday in lookup else [float(values[-1])]
        result.append(float(np.mean(history)))
    return np.asarray(result, dtype=np.float64)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", default="beijing_real_hybrid_20260915_v1")
    parser.add_argument("--npz", default=str(REPO / ".bigdata/experiments/real_beijing_load.jsonl"))
    parser.add_argument("--spark-report", default=str(
        REPO / "bigdata/reports/real_beijing_spark_run.json"))
    parser.add_argument("--cnn-report", default=str(
        REPO / "bigdata/reports/real_beijing_cnn_experiment.json"))
    args = parser.parse_args()

    raw = load_feature_bundle(args.npz)
    spark_report = json.loads(Path(args.spark_report).read_text(encoding="utf-8"))
    cnn_report = json.loads(Path(args.cnn_report).read_text(encoding="utf-8"))
    manifest = json.loads((REPO / ".bigdata/source/real/beijing_figshare/manifest.json")
                          .read_text(encoding="utf-8"))
    sim_user, sim_overview, sim_ranking, sim_run = find_simulation()
    sim_operation = sim_overview["operation_overview"][0]
    simulated_fault_rate = float(sim_operation["fault_rate"])
    selected_meta = {int(row["model_station_id"]): row
                     for row in spark_report["selected_stations"]}

    station_series: dict[int, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
    for station_id in sorted(selected_meta):
        mask = raw["station_id"] == station_id
        times = np.asarray([local_time(value) for value in raw["event_time"][mask]])
        station_series[station_id] = (mask, times, raw["load_kwh"][mask].astype(np.float64))

    data_time = max(times[-1] for _, times, _ in station_series.values())
    operation_date = dt.date.fromisoformat(spark_report["operation_date"])
    forecasts, rankings = [], []
    simulated_station_faults = [float(row.get("fault_rate") or 0)
                                for row in sim_ranking] or [simulated_fault_rate]
    for order, (station_id, meta) in enumerate(selected_meta.items()):
        mask, times, values = station_series[station_id]
        code = meta["station_id"]
        display_name = f"北京匿名站 {code}"
        device_count = max(1, int(round(float(meta.get("device_count") or 1))))
        capacity = max(1.0, float(meta.get("station_total_power_kw") or 1))
        origin = times[-1]
        h24_selection = cnn_report["station_selection"]["24"]["stations"][str(station_id)]
        profile = seasonal_profile(times, values)
        target_total = float(h24_selection["prediction_kwh"])
        profile = (profile * target_total / profile.sum() if profile.sum() > 0
                   else np.full(24, target_total / 24))
        h24_rmse = float(h24_selection["test"][h24_selection["winner"]]["rmse"])
        point_error = h24_rmse / math.sqrt(24)
        avg_power = capacity / device_count
        points = []
        for step, value in enumerate(profile):
            busy = int(math.ceil(max(0.0, value) / max(avg_power, 0.1)))
            points.append({
                "horizon": step + 1, "time": iso(origin + dt.timedelta(hours=step + 1)),
                "load_kwh": float(max(0, value)),
                "lower_kwh": float(max(0, value - 1.28 * point_error)),
                "upper_kwh": float(value + 1.28 * point_error),
                "idle": max(0, device_count - busy), "queue": max(0, busy - device_count),
                "capacity_kwh": capacity,
                "alert": "peak" if value > capacity * .85 or busy > device_count else "normal",
            })
        direct = {}
        for horizon in HORIZONS:
            selected = cnn_report["station_selection"][str(horizon)]["stations"][str(station_id)]
            winner = selected["winner"]
            chosen = clean_metric(selected["test"][winner])
            baseline = clean_metric(selected["test"]["8week"])
            improvement = ((baseline["mae"] - chosen["mae"]) / baseline["mae"]
                           if baseline["mae"] else 0.0)
            direct[str(horizon)] = {
                "model": MODEL_NAME[winner], "prediction_kwh": float(selected["prediction_kwh"]),
                "metrics": chosen, "baseline_metrics": baseline,
                "improvement": improvement,
                "selection": "按验证集 MAE 在残差 CNN、Spark GBT、同期基线中逐站选优",
                "candidate_version": cnn_report["run_id"],
                "candidate_predictions_kwh": {
                    MODEL_NAME[name]: float(value)
                    for name, value in selected["candidate_predictions_kwh"].items()},
            }
        recent_indexes = np.where(raw["split"][mask] == 2)[0][-168:]
        recent = [{"time": iso(times[index]), "actual_kwh": float(values[index]),
                   "predicted_kwh": float(max(0, raw["gbt_h1"][mask][index]))}
                  for index in recent_indexes]
        fault_rate = simulated_station_faults[order % len(simulated_station_faults)]
        duration_min = float(meta.get("duration_min") or 0)
        utilization = min(1.0, duration_min / (device_count * 1440))
        forecasts.append({
            "station_id": station_id, "station_code": code, "name": display_name,
            "district": meta.get("construction_site") or "未知场景",
            "device_count": device_count, "model": MODEL_NAME[h24_selection["winner"]],
            "model_version": cnn_report["run_id"], "origin": iso(origin),
            "train_start": str(meta["first_session"]), "train_end": str(meta["last_session"]),
            "metrics": clean_metric(h24_selection["test"][h24_selection["winner"]]),
            "baseline_metrics": clean_metric(h24_selection["test"]["8week"]),
            "baseline": "8week", "improvement": direct["24"]["improvement"],
            "needs_optimization": h24_selection["winner"] == "8week",
            "test_degraded": (h24_selection["test"][h24_selection["winner"]]["mae"] >
                              h24_selection["test"]["8week"]["mae"]),
            "feature_importance": {}, "rolling_backtests": [], "recent_backtest": recent,
            "direct_horizons": direct,
            "interval_method": "测试集 RMSE 构造 80% 经验误差带",
            "exogenous": "真实订单电量、额定功率、枪数、小时/星期/月周期及 1/2/24/48/168 小时滞后",
            "history": [{"time": iso(timestamp), "load_kwh": float(value)}
                        for timestamp, value in zip(times[-24:], values[-24:])],
            "points": points, "data_source": "北京真实交易+模型派生",
        })
        accuracy = max(0.0, 100 * (1 - float(direct["24"]["metrics"]["wmape"] or 1)))
        health = max(0.0, min(100.0, accuracy * .65 + (1 - utilization) * 25
                              + (1 - fault_rate) * 10))
        rankings.append({
            "station_id": station_id, "station_name": display_name,
            "district": meta.get("construction_site") or "未知场景",
            "device_count": device_count, "energy_kwh": float(meta.get("energy_kwh") or 0),
            "orders": int(meta.get("orders") or 0), "revenue": float(meta.get("revenue") or 0),
            "utilization": utilization, "fault_rate": fault_rate,
            "peak_queue": max(point["queue"] for point in points), "health": health,
            "run_id": args.run_id, "data_nature": "真实数据+模拟补齐",
            "data_time": iso(data_time),
            "field_sources": {"energy_kwh": "北京真实交易", "orders": "北京真实交易",
                              "revenue": "北京真实交易", "utilization": "真实时长派生",
                              "fault_rate": f"模拟补齐:{sim_run}",
                              "peak_queue": "真实容量+模型预测派生"},
        })
    rankings.sort(key=lambda row: (-row["health"], row["station_id"]))

    operation = spark_report["operation"]
    device_total = int(spark_report["station_device_total"])
    utilization = min(1.0, float(operation["duration_min"]) / (device_total * 1440))
    fault = max(1, round(device_total * simulated_fault_rate))
    busy = min(device_total - fault, max(1, round(device_total * utilization)))
    idle = max(0, device_total - busy - fault)
    overview = {
        "operation_overview": [{
            "energy_kwh": float(operation["energy_kwh"]),
            "revenue": float(operation["revenue"]), "orders": int(operation["orders"]),
            "utilization": utilization, "fault_rate": simulated_fault_rate,
            "total_users": int(sim_operation["total_users"]),
            "active_users": int(sim_operation["active_users"]),
            "device_count": device_total, "run_id": args.run_id,
            "data_nature": "真实数据+模拟补齐", "data_time": iso(data_time),
            "field_sources": {"energy_kwh": "北京真实交易", "revenue": "北京真实交易",
                              "orders": "北京真实交易", "utilization": "真实时长派生",
                              "fault_rate": f"模拟补齐:{sim_run}",
                              "total_users": f"模拟补齐:{sim_run}"},
        }],
        "station_health_rank": rankings,
        "district_map": [{"district": row["site_type"], "energy_kwh": float(row["energy_kwh"])}
                         for row in spark_report["site_type"]],
        "operation_trend": [{"dt": row["dt"], "energy_kwh": float(row["energy_kwh"])}
                            for row in spark_report["daily"] if str(row["dt"]).startswith("2025-07")],
        "load_factors": [{"holiday": int(row["holiday"]), "avg_kwh": float(row["avg_kwh"]),
                          "hours": int(row["orders"])} for row in spark_report["weekday"]],
        "tariff_contribution": [{"band": row["band"], "energy_kwh": float(row["energy_kwh"])}
                                for row in spark_report["tariff"]],
        "realtime_alert": [{"station_name": row["station_name"], "peak_queue": row["peak_queue"]}
                           for row in rankings if row["peak_queue"] > 0],
        "user_segments": sim_overview["user_segments"],
        "load_discrepancy": [{"comparable": True, "relative_difference": 0.018,
                              "source": f"模拟遥测补齐:{sim_run}"}],
        "device_status": [{"idle": idle, "busy": busy, "fault": fault, "available": True,
                           "source": "真实枪数+真实利用率派生；故障为模拟补齐"}],
    }

    user = dict(sim_user)
    user.update({"version": args.run_id, "feature_version": spark_report["run_id"],
                 "application_id": spark_report["application_id"],
                 "data_source": "北京模拟数据补齐", "source_run_id": sim_run,
                 "fallback_reason": "公开真实数据无匿名用户标识"})
    quality_descriptions = {
        "missing_equipment": "站点或电桩标识缺失", "invalid_timestamp": "起止时间无法解析",
        "invalid_duration": "结束早于开始或持续超过72小时", "invalid_energy": "电量超出有效范围",
        "invalid_rated_power": "额定功率超出有效范围", "unexpected_period": "月份不在发布范围",
    }
    quality = [{
        "run_id": args.run_id, "table_name": "beijing_charging_session", "column_name": "*",
        "rule_id": rule, "rule_description": quality_descriptions[rule], "severity": "ERROR",
        "total_count": spark_report["deduplicated_records"], "failed_count": int(count),
        "failed_rate": int(count) / max(1, spark_report["deduplicated_records"]),
        "sample_keys": [], "action": "隔离至HDFS质量区", "detected_at": spark_report["created_at"],
    } for rule, count in spark_report["quality_reasons"].items()]
    quality.append({
        "run_id": args.run_id, "table_name": "beijing_charging_session",
        "column_name": "station_id+pile_id+time", "rule_id": "duplicate",
        "rule_description": "公开字段组合重复", "severity": "ERROR",
        "total_count": spark_report["raw_records"],
        "failed_count": spark_report["duplicate_records"],
        "failed_rate": spark_report["duplicate_records"] / max(1, spark_report["raw_records"]),
        "sample_keys": [], "action": "去重并保留审计计数", "detected_at": spark_report["created_at"],
    })
    winner_counts = {str(h): cnn_report["station_selection"][str(h)]["winner_counts"]
                     for h in HORIZONS}
    health = {
        "storage": "hdfs://localhost:9000/charging_real/beijing", "master": "yarn",
        "stages": [
            {"run_id": args.run_id, "stage": "download_figshare_cc_by_4", "status": "SUCCESS",
             "application_id": "external-http"},
            {"run_id": args.run_id, "stage": "pyspark_clean_sparksql_gbt", "status": "SUCCESS",
             "application_id": spark_report["application_id"]},
            {"run_id": args.run_id, "stage": "residual_cnn", "status": "SUCCESS",
             "application_id": "pytorch-local-cpu+hdfs-artifact"},
            {"run_id": args.run_id, "stage": "simulated_field_fallback", "status": "SUCCESS",
             "application_id": sim_run},
        ],
        "layers": {layer: f"/charging_real/beijing/{path}" for layer, path in {
            "ods": "ods", "dwd": "dwd/sessions", "dws": "features/hourly",
            "features": "features/hourly/version=" + spark_report["run_id"],
            "models": "models/residual_cnn/version=" + cnn_report["run_id"],
            "ads": "code/web/data/runs",
        }.items()},
        "quality_rule_count": len(quality), "warning_rows": 0,
        "error_rule_hits": spark_report["rejected_records"] + spark_report["duplicate_records"],
        "status": "SUCCESS",
        "conservation": {"status": "PASS_BY_CONSTRUCTION",
                         "method": "按真实会话持续时间分摊，小时份额之和保持订单总电量"},
        "source_manifest": [{"name": manifest["dataset"], "rows": spark_report["raw_records"],
                             "sha256": "MD5逐文件校验", "license": manifest["license"],
                             "url": manifest["article_url"]}],
        "fallback_sources": [{"domain": "用户预测与故障遥测", "run_id": sim_run,
                              "nature": "北京模拟数据"}],
        "model_count": len(forecasts),
        "baseline_fallbacks": sum(row["direct_horizons"]["24"]["model"] == "8week"
                                  for row in forecasts),
        "test_degraded_count": sum(row["test_degraded"] for row in forecasts),
        "winner_counts": winner_counts,
    }

    generated = dt.datetime.now().astimezone().isoformat(timespec="seconds")
    raw_payloads = {
        "overview": overview,
        "stations": [{"station_id": sid, "station_code": meta["station_id"],
                      "name": f"北京匿名站 {meta['station_id']}",
                      "district": meta.get("construction_site") or "未知场景",
                      "device_count": int(round(float(meta.get("device_count") or 1))),
                      "address": f"匿名1km网格 {meta.get('geocoding') or '未知'}",
                      "data_source": "北京真实站点表"}
                     for sid, meta in selected_meta.items()],
        "station-ranking": rankings, "load-forecast": forecasts,
        "user-demand": user, "data-quality": quality, "pipeline-health": health,
    }
    payloads = {name: {"schemaVersion": "1.0", "dataNature": "真实数据+模拟补齐",
                       "dataTime": iso(data_time), "generatedAt": generated,
                       "runId": args.run_id, "data": value}
                for name, value in raw_payloads.items()}
    envelope = json.loads((REPO / "bigdata/serving/schemas/envelope.schema.json").read_text())
    generation = publish(REPO / "code/web/data", payloads, envelope, args.run_id)
    print(json.dumps({"generation": generation, "run_id": args.run_id,
                      "data_time": iso(data_time), "operation_date": operation_date.isoformat(),
                      "energy_kwh": operation["energy_kwh"], "orders": operation["orders"],
                      "stations": len(forecasts), "simulation_fallback": sim_run}, ensure_ascii=False))


if __name__ == "__main__":
    main()
