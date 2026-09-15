"""Clean real Boulder sessions with Spark SQL and train distributed GBT baselines.

Outputs a contiguous hourly station-load data set to HDFS and an NPZ bundle used
by the shared residual Temporal CNN experiment.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
from pathlib import Path

import numpy as np
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import GBTRegressor
from pyspark.sql import SparkSession, Window, functions as F


REPO = Path(__file__).resolve().parents[2]
HORIZONS = (1, 6, 24)
FEATURES = (
    "lag_1", "lag_2", "lag_24", "lag_48", "lag_168",
    "mean_24", "mean_168", "hour_sin", "hour_cos", "dow_sin", "dow_cos",
)


def duration_seconds(column: str):
    parts = F.split(F.col(column), ":")
    return (
        parts.getItem(0).cast("long") * 3600
        + parts.getItem(1).cast("long") * 60
        + parts.getItem(2).cast("double")
    )


def metric_rows(frame, target: str, prediction: str) -> dict:
    row = frame.select(
        F.count("*").alias("n"),
        F.avg(F.abs(F.col(prediction) - F.col(target))).alias("mae"),
        F.sqrt(F.avg(F.pow(F.col(prediction) - F.col(target), 2))).alias("rmse"),
        (F.sum(F.abs(F.col(prediction) - F.col(target))) /
         F.sum(F.abs(F.col(target)))).alias("wmape"),
    ).first()
    return {"n": int(row.n), "mae": float(row.mae), "rmse": float(row.rmse),
            "wmape": float(row.wmape) if row.wmape is not None else None}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default=str(
        REPO / ".bigdata/source/real/boulder/pages/*.jsonl.gz"))
    parser.add_argument("--root", default="hdfs://localhost:9000/charging_real/boulder")
    parser.add_argument("--output", default=str(
        REPO / ".bigdata/experiments/real_boulder_load.npz"))
    parser.add_argument("--run-id", default="real_" + dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    parser.add_argument("--max-stations", type=int, default=20)
    parser.add_argument("--min-sessions", type=int, default=500)
    parser.add_argument("--min-days", type=int, default=365)
    parser.add_argument("--validation-days", type=int, default=60)
    parser.add_argument("--test-days", type=int, default=60)
    args = parser.parse_args()

    spark = (SparkSession.builder.appName("charging-real-boulder-" + args.run_id)
             .config("spark.sql.session.timeZone", "America/Denver")
             .config("spark.sql.shuffle.partitions", "24")
             .config("spark.sql.adaptive.enabled", "true")
             .config("spark.ui.showConsoleProgress", "false")
             .getOrCreate())
    spark.sparkContext.setLogLevel("WARN")
    root = args.root.rstrip("/")

    input_path = str(Path(args.input).absolute())
    raw = spark.read.json("file://" + input_path)
    raw_count = raw.count()
    deduplicated = raw.dropDuplicates(["ObjectId2"])
    parsed = (deduplicated
        .withColumn("station_name", F.trim("Station_Name"))
        .withColumn("start_ts", F.to_timestamp("Start_Date___Time", "M/d/yyyy H:mm"))
        .withColumn("end_ts", F.to_timestamp("End_Date___Time", "M/d/yyyy H:mm"))
        .withColumn("energy_kwh", F.col("Energy__kWh_").cast("double"))
        .withColumn("charging_seconds", duration_seconds("Charging_Time__hh_mm_ss_"))
        .withColumn("total_seconds", F.col("end_ts").cast("long") - F.col("start_ts").cast("long")))
    valid_expression = (
        F.col("station_name").isNotNull() & (F.length("station_name") > 0)
        & F.col("start_ts").isNotNull() & F.col("end_ts").isNotNull()
        & (F.col("total_seconds") > 0) & (F.col("total_seconds") <= 72 * 3600)
        & F.col("energy_kwh").isNotNull() & (F.col("energy_kwh") > 0)
        & (F.col("energy_kwh") <= 300)
        & F.col("charging_seconds").isNotNull() & (F.col("charging_seconds") > 0)
        & (F.col("charging_seconds") <= F.col("total_seconds") + 300)
    )
    rejected = parsed.filter((~valid_expression) | valid_expression.isNull()).withColumn(
        "quality_reason",
        F.when(F.col("station_name").isNull() | (F.length("station_name") == 0), "missing_station")
         .when(F.col("start_ts").isNull() | F.col("end_ts").isNull(), "invalid_timestamp")
         .when((F.col("total_seconds") <= 0) | (F.col("total_seconds") > 72 * 3600), "invalid_duration")
         .when(F.col("energy_kwh").isNull() | (F.col("energy_kwh") <= 0) |
               (F.col("energy_kwh") > 300), "invalid_energy")
         .otherwise("invalid_charging_time"))
    cleaned = parsed.filter(valid_expression).cache()
    rejected.write.mode("overwrite").parquet(root + "/quality/rejected/run_id=" + args.run_id)
    cleaned.write.mode("overwrite").parquet(root + "/dwd/sessions/run_id=" + args.run_id)

    cleaned.createOrReplaceTempView("real_sessions")
    station_stats = spark.sql("""
        SELECT station_name, COUNT(*) AS session_count,
               MIN(start_ts) AS first_session, MAX(end_ts) AS last_session,
               DATEDIFF(MAX(end_ts), MIN(start_ts)) AS active_days,
               SUM(energy_kwh) AS energy_kwh
        FROM real_sessions GROUP BY station_name
    """)
    eligible = (station_stats
        .filter((F.col("session_count") >= args.min_sessions) &
                (F.col("active_days") >= args.min_days))
        .orderBy(F.desc("session_count"), "station_name")
        .limit(args.max_stations)
        .select("station_name", "session_count", "first_session", "last_session",
                "active_days", "energy_kwh")
        .withColumn("station_id", F.row_number().over(Window.orderBy("station_name"))))
    selected = cleaned.join(F.broadcast(eligible.select("station_name", "station_id")),
                            "station_name", "inner")

    # The source reports total delivered energy and cumulative charging time, not
    # an intra-session power curve.  Allocate energy uniformly from session start
    # across the reported charging duration and preserve total kWh exactly.
    charge_end_epoch = F.least(
        F.col("end_ts").cast("long"),
        F.col("start_ts").cast("long") + F.col("charging_seconds").cast("long"))
    allocated = (selected
        .withColumn("charge_end_ts", F.from_unixtime(charge_end_epoch).cast("timestamp"))
        .withColumn("event_hour", F.explode(F.sequence(
            F.date_trunc("hour", "start_ts"),
            F.date_trunc("hour", "charge_end_ts"), F.expr("INTERVAL 1 HOUR"))))
        .withColumn("overlap_seconds", F.greatest(F.lit(0),
            F.least(F.col("charge_end_ts").cast("long"),
                    (F.col("event_hour") + F.expr("INTERVAL 1 HOUR")).cast("long"))
            - F.greatest(F.col("start_ts").cast("long"), F.col("event_hour").cast("long"))))
        .filter(F.col("overlap_seconds") > 0)
        .withColumn("allocated_kwh", F.col("energy_kwh") * F.col("overlap_seconds") /
                    F.col("charging_seconds")))
    allocated.createOrReplaceTempView("allocated_sessions")
    observed = spark.sql("""
        SELECT station_id, station_name, event_hour,
               SUM(allocated_kwh) AS load_kwh,
               COUNT(DISTINCT ObjectId2) AS active_sessions
        FROM allocated_sessions GROUP BY station_id, station_name, event_hour
    """)
    bounds = observed.groupBy("station_id", "station_name").agg(
        F.min("event_hour").alias("first_hour"), F.max("event_hour").alias("last_hour"))
    grid = bounds.select("station_id", "station_name", F.explode(F.sequence(
        "first_hour", "last_hour", F.expr("INTERVAL 1 HOUR"))).alias("event_hour"))
    hourly = (grid.join(observed, ["station_id", "station_name", "event_hour"], "left")
              .fillna({"load_kwh": 0.0, "active_sessions": 0}).cache())

    station_window = Window.partitionBy("station_id").orderBy("event_hour")
    history_24 = station_window.rowsBetween(-24, -1)
    history_168 = station_window.rowsBetween(-168, -1)
    feature_data = (hourly
        .withColumn("lag_1", F.lag("load_kwh", 1).over(station_window))
        .withColumn("lag_2", F.lag("load_kwh", 2).over(station_window))
        .withColumn("lag_24", F.lag("load_kwh", 24).over(station_window))
        .withColumn("lag_48", F.lag("load_kwh", 48).over(station_window))
        .withColumn("lag_168", F.lag("load_kwh", 168).over(station_window))
        .withColumn("mean_24", F.avg("load_kwh").over(history_24))
        .withColumn("mean_168", F.avg("load_kwh").over(history_168))
        .withColumn("hour_sin", F.sin(F.hour("event_hour") * F.lit(2 * np.pi / 24)))
        .withColumn("hour_cos", F.cos(F.hour("event_hour") * F.lit(2 * np.pi / 24)))
        .withColumn("dow_sin", F.sin((F.dayofweek("event_hour") - 1) * F.lit(2 * np.pi / 7)))
        .withColumn("dow_cos", F.cos((F.dayofweek("event_hour") - 1) * F.lit(2 * np.pi / 7))))
    for horizon in HORIZONS:
        future = station_window.rowsBetween(0, horizon - 1)
        feature_data = (feature_data
            .withColumn(f"target_h{horizon}", F.sum("load_kwh").over(future))
            .withColumn(f"target_count_h{horizon}", F.count("load_kwh").over(future)))
    station_last = Window.partitionBy("station_id")
    feature_data = (feature_data
        .withColumn("station_last", F.max("event_hour").over(station_last))
        .withColumn("test_start", F.expr(f"station_last - INTERVAL {args.test_days} DAYS"))
        .withColumn("validation_start", F.expr(
            f"station_last - INTERVAL {args.test_days + args.validation_days} DAYS"))
        .withColumn("split", F.when(F.col("event_hour") < F.col("validation_start"), "train")
                    .when(F.col("event_hour") < F.col("test_start"), "validation")
                    .otherwise("test"))
        .dropna(subset=list(FEATURES)).cache())

    assembler = VectorAssembler(inputCols=list(FEATURES), outputCol="features")
    vectorized = assembler.transform(feature_data).cache()
    scored = feature_data.select("station_id", "station_name", "event_hour", "load_kwh", "split")
    evaluations: dict[str, dict[str, dict]] = {}
    for horizon in HORIZONS:
        target = f"target_h{horizon}"
        usable = vectorized.filter(F.col(f"target_count_h{horizon}") == horizon)
        train = usable.filter(F.col("split") == "train")
        model = GBTRegressor(
            featuresCol="features", labelCol=target, predictionCol=f"gbt_h{horizon}",
            maxIter=30, maxDepth=5, maxBins=64, stepSize=0.05, seed=20260915,
            lossType="absolute") .fit(train)
        model_path = root + f"/models/gbt/horizon={horizon}/version={args.run_id}"
        model.write().overwrite().save(model_path)
        predicted = model.transform(vectorized).select(
            "station_id", "event_hour", F.greatest(F.lit(0.0), F.col(f"gbt_h{horizon}"))
            .alias(f"gbt_h{horizon}"))
        scored = scored.join(predicted, ["station_id", "event_hour"], "inner")
        evaluations[str(horizon)] = {}
        evaluated = model.transform(usable)
        for split in ("validation", "test"):
            evaluations[str(horizon)][split] = metric_rows(
                evaluated.filter(F.col("split") == split), target, f"gbt_h{horizon}")

    final = scored.orderBy("station_id", "event_hour").cache()
    final.write.mode("overwrite").parquet(root + "/features/hourly/version=" + args.run_id)
    rows = final.collect()
    split_codes = {"train": 0, "validation": 1, "test": 2}
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        output,
        station_id=np.asarray([r.station_id for r in rows], dtype=np.int32),
        event_time=np.asarray([int(r.event_hour.timestamp()) for r in rows], dtype=np.int64),
        local_hour=np.asarray([r.event_hour.hour for r in rows], dtype=np.int8),
        local_dow=np.asarray([r.event_hour.weekday() for r in rows], dtype=np.int8),
        load_kwh=np.asarray([float(r.load_kwh) for r in rows], dtype=np.float32),
        device_count=np.ones(len(rows), dtype=np.float32),
        facility_type=np.zeros(len(rows), dtype=np.int16),
        split=np.asarray([split_codes[r.split] for r in rows], dtype=np.int8),
        gbt_h1=np.asarray([float(r.gbt_h1) for r in rows], dtype=np.float32),
        gbt_h6=np.asarray([float(r.gbt_h6) for r in rows], dtype=np.float32),
        gbt_h24=np.asarray([float(r.gbt_h24) for r in rows], dtype=np.float32),
    )
    selected_stations = [row.asDict(recursive=True) for row in eligible.orderBy("station_id").collect()]
    quality_counts = {row.quality_reason: row["count"] for row in
                      rejected.groupBy("quality_reason").count().collect()}
    report = {
        "run_id": args.run_id,
        "source": "City of Boulder Electric Vehicle Charging Station Data",
        "source_manifest": str(REPO / ".bigdata/source/real/boulder/manifest.json"),
        "raw_records": raw_count,
        "deduplicated_records": deduplicated.count(),
        "clean_records": cleaned.count(),
        "rejected_records": rejected.count(),
        "quality_reasons": quality_counts,
        "selected_stations": selected_stations,
        "hourly_rows": len(rows),
        "split_counts": {row.split: row["count"] for row in final.groupBy("split").count().collect()},
        "gbt_evaluation": evaluations,
        "hdfs_feature_path": root + "/features/hourly/version=" + args.run_id,
        "npz_output": str(output),
        "application_id": spark.sparkContext.applicationId,
        "spark_master": spark.sparkContext.master,
        "energy_allocation_assumption": (
            "Session kWh is distributed uniformly from start time over reported charging duration."
        ),
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    report_path = REPO / "bigdata/reports/real_boulder_spark_run.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2, default=str),
                           encoding="utf-8")
    print(json.dumps({"report": str(report_path), "rows": len(rows),
                      "stations": len(selected_stations)}, ensure_ascii=False))
    spark.stop()


if __name__ == "__main__":
    main()
