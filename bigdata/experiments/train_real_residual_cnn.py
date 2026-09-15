"""Train a shared residual Temporal CNN on an external real station series."""
from __future__ import annotations

import argparse
import copy
import datetime as dt
import json
import math
import os
import random
import subprocess
import sys
from pathlib import Path

import numpy as np
import torch
from torch import nn


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from train_temporal_cnn import (  # noqa: E402
    HORIZONS, TemporalCNN, batches, build_origins, build_samples, metric,
    predict, tensors, load_array_bundle,
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default=str(
        REPO / ".bigdata/experiments/real_boulder_load.npz"))
    parser.add_argument("--spark-report", default=str(
        REPO / "bigdata/reports/real_boulder_spark_run.json"))
    parser.add_argument("--run-id", default="real_cnn_" + dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    parser.add_argument("--epochs", type=int, default=15)
    parser.add_argument("--batch-size", type=int, default=1024)
    parser.add_argument("--train-stride", type=int, default=3)
    parser.add_argument("--hdfs-root", default="/charging_real/boulder")
    parser.add_argument("--dataset-name", default="City of Boulder Electric Vehicle Charging Station Data")
    parser.add_argument("--report-output", default=str(
        REPO / "bigdata/reports/real_boulder_cnn_experiment.json"))
    parser.add_argument("--dashboard-output", default=str(
        REPO / "code/web/data/real-load-forecast.json"))
    args = parser.parse_args()

    seed = 20260915
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.set_num_threads(min(8, os.cpu_count() or 4))
    raw = load_array_bundle(args.input)
    spark_report = json.loads(Path(args.spark_report).read_text(encoding="utf-8"))
    station_ids = sorted(int(value) for value in np.unique(raw["station_id"]))
    station_lookup = {station_id: index for index, station_id in enumerate(station_ids)}
    station_names = {
        int(row.get("model_station_id", row["station_id"])):
        row.get("station_name", "北京匿名站 " + str(row["station_id"]))
        for row in spark_report["selected_stations"]
    }

    train = build_samples(raw, "train", station_lookup, args.train_stride)
    validation = build_samples(raw, "validation", station_lookup, 1)
    test = build_samples(raw, "test", station_lookup, 1)
    origins = build_origins(raw, station_lookup)
    if not min(len(train["x"]), len(validation["x"]), len(test["x"])):
        raise ValueError("At least one split has no eligible contiguous samples")

    x_mean, x_std = float(train["x"].mean()), float(train["x"].std() + 1e-6)
    residual_target = train["target"] - train["baseline"]
    y_mean = residual_target.mean(axis=0).astype(np.float32)
    y_std = (residual_target.std(axis=0) + 1e-6).astype(np.float32)
    baseline_mean = train["baseline"].mean(axis=0).astype(np.float32)
    baseline_std = (train["baseline"].std(axis=0) + 1e-6).astype(np.float32)

    model = TemporalCNN(len(station_ids), residual=True)
    optimizer = torch.optim.AdamW(model.parameters(), lr=2e-3, weight_decay=1e-4)
    loss_fn = nn.SmoothL1Loss()
    history, best_state, best_score, patience = [], None, math.inf, 0
    for epoch in range(1, args.epochs + 1):
        model.train()
        losses = []
        for indexes in batches(train, args.batch_size, True, seed + epoch):
            x, station, calendar, baseline, target = tensors(
                train, indexes, x_mean, x_std, y_mean, y_std,
                baseline_mean, baseline_std, True)
            optimizer.zero_grad(set_to_none=True)
            loss = loss_fn(model(x, station, calendar, baseline), target)
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 2.0)
            optimizer.step()
            losses.append(float(loss.detach()))
        validation_prediction = predict(
            model, validation, x_mean, x_std, y_mean, y_std,
            baseline_mean, baseline_std, True, args.batch_size)
        normalized_mae = float(np.mean(
            np.abs(validation_prediction - validation["target"]) / y_std))
        history.append({"epoch": epoch, "train_loss": float(np.mean(losses)),
                        "validation_normalized_mae": normalized_mae})
        print(json.dumps(history[-1]), flush=True)
        if normalized_mae < best_score - 1e-4:
            best_score, best_state, patience = normalized_mae, copy.deepcopy(model.state_dict()), 0
        else:
            patience += 1
            if patience >= 3:
                break
    if best_state is None:
        raise RuntimeError("No model checkpoint was produced")
    model.load_state_dict(best_state)

    predictions = {name: predict(
        model, data, x_mean, x_std, y_mean, y_std,
        baseline_mean, baseline_std, True, args.batch_size)
        for name, data in (("validation", validation), ("test", test))}
    origin_prediction = predict(
        model, origins, x_mean, x_std, y_mean, y_std,
        baseline_mean, baseline_std, True, args.batch_size)

    evaluation: dict[str, dict] = {"validation": {}, "test": {}}
    station_selection: dict[str, dict] = {}
    forecasts = []
    for column, horizon in enumerate(HORIZONS):
        per_model = {}
        for split_name, data in (("validation", validation), ("test", test)):
            candidates = {
                "residual_cnn": predictions[split_name][:, column],
                "spark_gbt": data["gbt"][:, column],
                "8week": data["baseline"][:, column],
            }
            evaluation[split_name][str(horizon)] = {
                name: metric(data["target"][:, column], values, data["device"])
                for name, values in candidates.items()
            }
            per_model[split_name] = {}
            for name, values in candidates.items():
                per_model[split_name][name] = {}
                for index, station_id in enumerate(station_ids):
                    mask = data["station"] == index
                    per_model[split_name][name][str(station_id)] = metric(
                        data["target"][mask, column], values[mask], data["device"][mask])
            validation_mae = {
                name: evaluation[split_name][str(horizon)][name]["mae"]
                for name in candidates
            }
            evaluation[split_name][str(horizon)]["best"] = min(
                validation_mae, key=validation_mae.get)

        stations = {}
        for index, station_id in enumerate(station_ids):
            sid = str(station_id)
            winner = min(("residual_cnn", "spark_gbt", "8week"),
                         key=lambda name: per_model["validation"][name][sid]["mae"])
            raw_mask = raw["station_id"] == station_id
            origin_candidates = {
                "residual_cnn": float(origin_prediction[index, column]),
                "spark_gbt": float(raw[f"gbt_h{horizon}"][raw_mask][-1]),
                "8week": float(origins["baseline"][index, column]),
            }
            stations[sid] = {
                "station_name": station_names.get(station_id, sid),
                "winner": winner,
                "prediction_kwh": origin_candidates[winner],
                "candidate_predictions_kwh": origin_candidates,
                "validation": {name: per_model["validation"][name][sid]
                               for name in origin_candidates},
                "test": {name: per_model["test"][name][sid]
                         for name in origin_candidates},
            }
            forecasts.append({"station_id": station_id,
                              "station_name": station_names.get(station_id, sid),
                              "horizon_hours": horizon, "model": winner,
                              "prediction_kwh": origin_candidates[winner]})
        station_selection[str(horizon)] = {
            "stations": stations,
            "winner_counts": {name: sum(row["winner"] == name for row in stations.values())
                              for name in ("residual_cnn", "spark_gbt", "8week")},
        }

    output_dir = REPO / ".bigdata/experiments"
    output_dir.mkdir(parents=True, exist_ok=True)
    model_path = output_dir / f"{args.run_id}.pt"
    torch.save({
        "state_dict": model.state_dict(), "station_ids": station_ids,
        "station_names": station_names, "x_mean": x_mean, "x_std": x_std,
        "y_mean": y_mean, "y_std": y_std, "baseline_mean": baseline_mean,
        "baseline_std": baseline_std, "residual": True, "run_id": args.run_id,
    }, model_path)
    report = {
        "run_id": args.run_id,
        "status": "TRAINED_EXTERNAL_REAL_DATA",
        "dataset": args.dataset_name,
        "algorithm": "weekly seasonal baseline + shared residual Temporal CNN",
        "lookback_hours": 168,
        "horizons": list(HORIZONS),
        "training_runtime": (
            "Spark GBT and feature engineering ran on YARN/Hadoop; residual CNN ran "
            "with PyTorch on the local CPU and artifacts were archived to HDFS."
        ),
        "sample_counts": {name: len(data["x"]) for name, data in
                          (("train", train), ("validation", validation), ("test", test))},
        "epochs_completed": len(history),
        "history": history,
        "evaluation": evaluation,
        "station_selection": station_selection,
        "forecasts": forecasts,
        "selection_rule": "Per-station winner is selected only by validation MAE; test is report-only.",
        "spark_run_id": spark_report["run_id"],
        "spark_application_id": spark_report["application_id"],
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    report_path = Path(args.report_output).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    hdfs_version = args.hdfs_root.rstrip("/") + "/models/residual_cnn/version=" + args.run_id
    subprocess.run(["hdfs", "dfs", "-mkdir", "-p", hdfs_version], check=True)
    subprocess.run(["hdfs", "dfs", "-put", "-f", str(model_path), hdfs_version + "/model.pt"],
                   check=True)
    subprocess.run(["hdfs", "dfs", "-put", "-f", str(report_path),
                    hdfs_version + "/evaluation.json"], check=True)
    dashboard_path = Path(args.dashboard_output).resolve()
    dashboard_path.parent.mkdir(parents=True, exist_ok=True)
    dashboard_path.write_text(json.dumps({
        "source": args.dataset_name,
        "run_id": args.run_id, "data": forecasts,
    }, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"report": str(report_path), "model": str(model_path),
                      "hdfs": hdfs_version}, ensure_ascii=False))


if __name__ == "__main__":
    main()
