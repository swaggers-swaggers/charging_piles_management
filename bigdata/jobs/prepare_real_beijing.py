"""Clean the public Beijing transactions with PySpark and train Spark GBTs.

The publisher stores timestamps as Parquet TIMESTAMP_NANOS.  Spark 3.4 reads
those physical values as int64 with ``nanosAsLong`` and this job converts them
to timestamps explicitly.  January and July are kept as separate contiguous
periods so the five-month publication gap is never interpreted as zero load.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import math
from pathlib import Path

from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import GBTRegressionModel, GBTRegressor
from pyspark.sql import SparkSession, Window, functions as F


REPO = Path(__file__).resolve().parents[2]
HORIZONS = (1, 6, 24)
FEATURES = (
    "model_station_id", "device_count", "station_total_power_kw", "avg_rated_power",
    "lag_1", "lag_2", "lag_24", "lag_48", "lag_168", "mean_24", "mean_168",
    "hour_sin", "hour_cos", "dow_sin", "dow_cos", "month_sin", "month_cos",
)


def metrics(frame, target: str, prediction: str) -> dict:
    row = frame.select(
        F.count("*").alias("n"),
        F.avg(F.abs(F.col(prediction) - F.col(target))).alias("mae"),
        F.sqrt(F.avg(F.pow(F.col(prediction) - F.col(target), 2))).alias("rmse"),
        (F.sum(F.abs(F.col(prediction) - F.col(target))) /
         F.sum(F.abs(F.col(target)))).alias("wmape"),
    ).first()
    return {"n": int(row.n), "mae": float(row.mae), "rmse": float(row.rmse),
            "wmape": float(row.wmape) if row.wmape is not None else None}


def rows(frame) -> list[dict]:
    return [row.asDict(recursive=True) for row in frame.collect()]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-root", default="hdfs://localhost:9000/charging_real/beijing/ods")
    parser.add_argument("--root", default="hdfs://localhost:9000/charging_real/beijing")
    parser.add_argument("--output", default=str(
        REPO / ".bigdata/experiments/real_beijing_load.jsonl"))
    parser.add_argument("--run-id", default="beijing_spark_" + dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    parser.add_argument("--max-stations", type=int, default=20)
    parser.add_argument("--min-orders", type=int, default=2000)
    parser.add_argument("--reuse-model-run-id", default="",
                        help="Load already trained HDFS GBTs from this run id")
    args = parser.parse_args()

    spark = (SparkSession.builder.appName("charging-real-beijing-" + args.run_id)
             .config("spark.sql.session.timeZone", "Asia/Shanghai")
             .config("spark.sql.legacy.parquet.nanosAsLong", "true")
             .config("spark.sql.shuffle.partitions", "48")
             .config("spark.sql.adaptive.enabled", "true")
             .config("spark.ui.showConsoleProgress", "false")
             .getOrCreate())
    spark.sparkContext.setLogLevel("WARN")
    root = args.root.rstrip("/")
    input_root = args.input_root.rstrip("/")

    raw = spark.read.parquet(input_root + "/orders_*.parquet")
    stations = spark.read.parquet(input_root + "/stations_public.parquet").cache()
    raw_count = raw.count()
    station_count = stations.count()

    # No order id is released.  A pile cannot legitimately start two identical
    # sessions at the same nanosecond, so this composite is the safest public key.
    dedup_columns = ["station_id", "pile_id", "charge_start_time", "charge_end_time",
                     "total_elec_kwh", "total_fee"]
    deduplicated = raw.dropDuplicates(dedup_columns)
    parsed = (deduplicated
        .withColumn("start_ts", F.expr(
            "timestamp_micros(CAST(charge_start_time / 1000 AS BIGINT))"))
        .withColumn("end_ts", F.expr(
            "timestamp_micros(CAST(charge_end_time / 1000 AS BIGINT))"))
        .withColumn("duration_seconds", F.col("end_ts").cast("long") - F.col("start_ts").cast("long"))
        .withColumn("period", F.date_format("transaction_date", "yyyy-MM"))
        .withColumn("order_key", F.sha2(F.concat_ws("|", *[F.col(c).cast("string")
                                                            for c in dedup_columns]), 256)))
    valid = (
        F.col("station_id").isNotNull() & F.col("pile_id").isNotNull()
        & F.col("start_ts").isNotNull() & F.col("end_ts").isNotNull()
        & (F.col("duration_seconds") > 0) & (F.col("duration_seconds") <= 72 * 3600)
        & F.col("total_elec_kwh").isNotNull() & (F.col("total_elec_kwh") > 0)
        & (F.col("total_elec_kwh") <= 1000)
        & F.col("rated_gun_power_kw").isNotNull() & (F.col("rated_gun_power_kw") > 0)
        & (F.col("rated_gun_power_kw") <= 1000)
        & F.col("period").isin("2025-01", "2025-07")
    )
    rejected = (parsed.filter((~valid) | valid.isNull())
        .withColumn("quality_reason",
            F.when(F.col("station_id").isNull() | F.col("pile_id").isNull(), "missing_equipment")
             .when(F.col("start_ts").isNull() | F.col("end_ts").isNull(), "invalid_timestamp")
             .when((F.col("duration_seconds") <= 0) |
                   (F.col("duration_seconds") > 72 * 3600), "invalid_duration")
             .when(F.col("total_elec_kwh").isNull() | (F.col("total_elec_kwh") <= 0) |
                   (F.col("total_elec_kwh") > 1000), "invalid_energy")
             .when(F.col("rated_gun_power_kw").isNull() | (F.col("rated_gun_power_kw") <= 0) |
                   (F.col("rated_gun_power_kw") > 1000), "invalid_rated_power")
             .otherwise("unexpected_period")))
    cleaned = parsed.filter(valid).join(stations, "station_id", "left").cache()
    clean_count = cleaned.count()
    rejected_count = rejected.count()
    duplicates = raw_count - deduplicated.count()
    rejected.write.mode("overwrite").parquet(root + "/quality/rejected/run_id=" + args.run_id)
    cleaned.write.mode("overwrite").parquet(root + "/dwd/sessions/run_id=" + args.run_id)

    cleaned.createOrReplaceTempView("beijing_sessions")
    # SparkSQL supplies the operational aggregates consumed by the Web screen.
    daily = spark.sql("""
        SELECT transaction_date AS dt, COUNT(*) AS orders,
               SUM(total_elec_kwh) AS energy_kwh,
               SUM(COALESCE(total_fee, 0D)) AS revenue,
               SUM(duration_seconds) / 60D AS duration_min
        FROM beijing_sessions GROUP BY transaction_date ORDER BY dt
    """).cache()
    latest_date = daily.agg(F.max("dt")).first()[0]
    operation_date = latest_date - dt.timedelta(days=1)
    operation = daily.filter(F.col("dt") == F.lit(operation_date)).first().asDict()
    site_type = spark.sql(f"""
        SELECT COALESCE(construction_site, '未知场景') AS site_type,
               COUNT(*) AS orders, SUM(total_elec_kwh) AS energy_kwh
        FROM beijing_sessions WHERE transaction_date = DATE'{operation_date.isoformat()}'
        GROUP BY COALESCE(construction_site, '未知场景')
        ORDER BY energy_kwh DESC LIMIT 10
    """)
    tariff = spark.sql("""
        SELECT CASE WHEN HOUR(start_ts) < 6 THEN '夜间(00–06)'
                    WHEN HOUR(start_ts) < 18 THEN '日间(06–18)'
                    ELSE '晚间(18–24)' END AS band,
               SUM(total_elec_kwh) AS energy_kwh
        FROM beijing_sessions WHERE period='2025-07' GROUP BY 1
    """)
    weekday = spark.sql("""
        SELECT CASE WHEN DAYOFWEEK(start_ts) IN (1,7) THEN 1 ELSE 0 END AS holiday,
               COUNT(*) AS orders, AVG(total_elec_kwh) AS avg_kwh
        FROM beijing_sessions GROUP BY 1 ORDER BY 1
    """)

    station_stats = spark.sql("""
        SELECT station_id, COUNT(*) AS session_count, COUNT(DISTINCT period) AS period_count,
               MIN(start_ts) AS first_session, MAX(end_ts) AS last_session,
               SUM(total_elec_kwh) AS history_energy_kwh,
               AVG(rated_gun_power_kw) AS avg_rated_power
        FROM beijing_sessions GROUP BY station_id
    """)
    eligible = (station_stats
        .filter((F.col("session_count") >= args.min_orders) & (F.col("period_count") == 2))
        .orderBy(F.desc("session_count"), "station_id").limit(args.max_stations)
        .join(stations, "station_id", "left")
        .withColumn("model_station_id", F.row_number().over(Window.orderBy("station_id")))
        .withColumn("device_count", F.greatest(F.lit(1.0), F.coalesce(
            F.col("charging_gun_num"), F.col("piles_num"), F.lit(1.0))))
        .withColumn("station_total_power_kw", F.greatest(F.lit(1.0), F.coalesce(
            F.col("station_total_power_kw"), F.col("avg_rated_power"), F.lit(1.0))))
        .cache())
    if eligible.count() < 3:
        raise RuntimeError("Fewer than three stations satisfy the two-period training criteria")
    # ``cleaned`` already contains the publisher's raw station power column
    # from the metadata join.  Drop it before attaching the normalized
    # modelling capacity, otherwise SparkSQL sees two identically named
    # columns in the hourly allocation view.
    selected = cleaned.drop("station_total_power_kw").join(F.broadcast(eligible.select(
        "station_id", "model_station_id", "device_count", "station_total_power_kw")),
        "station_id", "inner")

    # Allocate each session's delivered kWh by its actual overlap with each hour.
    allocated = (selected
        .withColumn("event_hour", F.explode(F.sequence(
            F.date_trunc("hour", "start_ts"), F.date_trunc("hour", "end_ts"),
            F.expr("INTERVAL 1 HOUR"))))
        .withColumn("overlap_seconds", F.greatest(F.lit(0),
            F.least(F.col("end_ts").cast("long"),
                    (F.col("event_hour") + F.expr("INTERVAL 1 HOUR")).cast("long"))
            - F.greatest(F.col("start_ts").cast("long"), F.col("event_hour").cast("long"))))
        .filter(F.col("overlap_seconds") > 0)
        .withColumn("allocated_kwh", F.col("total_elec_kwh") *
                    F.col("overlap_seconds") / F.col("duration_seconds")))
    allocated.createOrReplaceTempView("allocated_beijing_sessions")
    observed = spark.sql("""
        SELECT model_station_id, station_id, period, event_hour,
               MAX(device_count) AS device_count,
               MAX(station_total_power_kw) AS station_total_power_kw,
               AVG(rated_gun_power_kw) AS avg_rated_power,
               SUM(allocated_kwh) AS load_kwh,
               COUNT(DISTINCT order_key) AS active_sessions
        FROM allocated_beijing_sessions
        GROUP BY model_station_id, station_id, period, event_hour
    """)
    bounds = observed.groupBy("model_station_id", "station_id", "period").agg(
        F.min("event_hour").alias("first_hour"), F.max("event_hour").alias("last_hour"),
        F.max("device_count").alias("device_count"),
        F.max("station_total_power_kw").alias("station_total_power_kw"),
        F.avg("avg_rated_power").alias("avg_rated_power"))
    grid = bounds.select("model_station_id", "station_id", "period", "device_count",
                         "station_total_power_kw", "avg_rated_power", F.explode(F.sequence(
        "first_hour", "last_hour", F.expr("INTERVAL 1 HOUR"))).alias("event_hour"))
    hourly = (grid.join(observed.select("model_station_id", "station_id", "period", "event_hour",
                                        "load_kwh", "active_sessions"),
                        ["model_station_id", "station_id", "period", "event_hour"], "left")
              .fillna({"load_kwh": 0.0, "active_sessions": 0}).cache())

    order_window = Window.partitionBy("model_station_id", "period").orderBy("event_hour")
    history_24 = order_window.rowsBetween(-24, -1)
    history_168 = order_window.rowsBetween(-168, -1)
    feature_data = (hourly
        .withColumn("lag_1", F.lag("load_kwh", 1).over(order_window))
        .withColumn("lag_2", F.lag("load_kwh", 2).over(order_window))
        .withColumn("lag_24", F.lag("load_kwh", 24).over(order_window))
        .withColumn("lag_48", F.lag("load_kwh", 48).over(order_window))
        .withColumn("lag_168", F.lag("load_kwh", 168).over(order_window))
        .withColumn("mean_24", F.avg("load_kwh").over(history_24))
        .withColumn("mean_168", F.avg("load_kwh").over(history_168))
        .withColumn("hour_sin", F.sin(F.hour("event_hour") * F.lit(2 * math.pi / 24)))
        .withColumn("hour_cos", F.cos(F.hour("event_hour") * F.lit(2 * math.pi / 24)))
        .withColumn("dow_sin", F.sin((F.dayofweek("event_hour") - 1) * F.lit(2 * math.pi / 7)))
        .withColumn("dow_cos", F.cos((F.dayofweek("event_hour") - 1) * F.lit(2 * math.pi / 7)))
        .withColumn("month_sin", F.sin(F.month("event_hour") * F.lit(2 * math.pi / 12)))
        .withColumn("month_cos", F.cos(F.month("event_hour") * F.lit(2 * math.pi / 12))))
    for horizon in HORIZONS:
        future = order_window.rowsBetween(0, horizon - 1)
        feature_data = (feature_data
            .withColumn(f"target_h{horizon}", F.sum("load_kwh").over(future))
            .withColumn(f"target_count_h{horizon}", F.count("load_kwh").over(future)))
    feature_data = (feature_data
        .withColumn("day", F.dayofmonth("event_hour"))
        .withColumn("split", F.when(F.col("day") <= 23, "train")
                    .when(F.col("day") <= 27, "validation").otherwise("test"))
        .dropna(subset=list(FEATURES)).cache())
    vectorized = VectorAssembler(inputCols=list(FEATURES), outputCol="features").transform(
        feature_data).cache()
    scored = feature_data.select("model_station_id", "station_id", "period", "event_hour",
                                 "load_kwh", "active_sessions", "device_count",
                                 "station_total_power_kw", "split")
    evaluations: dict[str, dict] = {}
    for horizon in HORIZONS:
        usable = vectorized.filter(F.col(f"target_count_h{horizon}") == horizon)
        train = usable.filter(F.col("split") == "train")
        if args.reuse_model_run_id:
            model = GBTRegressionModel.load(
                root + f"/models/gbt/horizon={horizon}/version={args.reuse_model_run_id}")
            model.setPredictionCol(f"gbt_h{horizon}")
        else:
            model = GBTRegressor(
                featuresCol="features", labelCol=f"target_h{horizon}",
                predictionCol=f"gbt_h{horizon}", maxIter=36, maxDepth=6, maxBins=64,
                stepSize=0.05, seed=20260915, lossType="absolute").fit(train)
            model.write().overwrite().save(
                root + f"/models/gbt/horizon={horizon}/version={args.run_id}")
        predictions = model.transform(vectorized).select(
            "model_station_id", "period", "event_hour",
            F.greatest(F.lit(0.0), F.col(f"gbt_h{horizon}")).alias(f"gbt_h{horizon}"))
        scored = scored.join(predictions, ["model_station_id", "period", "event_hour"], "inner")
        evaluated = model.transform(usable)
        evaluations[str(horizon)] = {
            split: metrics(evaluated.filter(F.col("split") == split),
                           f"target_h{horizon}", f"gbt_h{horizon}")
            for split in ("validation", "test")
        }

    final = scored.orderBy("model_station_id", "event_hour").cache()
    final.write.mode("overwrite").parquet(root + "/features/hourly/version=" + args.run_id)
    collected = final.collect()
    split_codes = {"train": 0, "validation": 1, "test": 2}
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as stream:
        for r in collected:
            stream.write(json.dumps({
                "station_id": int(r.model_station_id), "station_code": r.station_id,
                "event_time": int(r.event_hour.timestamp()), "local_hour": r.event_hour.hour,
                "local_dow": r.event_hour.weekday(), "load_kwh": float(r.load_kwh),
                "active_sessions": int(r.active_sessions),
                # CNN/GBT targets are station-level kWh.  Keep the shared
                # trainer scaling factor at one; carry real guns separately.
                "device_count": 1.0, "physical_device_count": float(r.device_count),
                "capacity_kw": float(r.station_total_power_kw),
                "split": split_codes[r.split], "gbt_h1": float(r.gbt_h1),
                "gbt_h6": float(r.gbt_h6), "gbt_h24": float(r.gbt_h24),
            }, ensure_ascii=False) + "\n")

    selected_ids = eligible.select("station_id")
    selected_day = (cleaned.join(F.broadcast(selected_ids), "station_id", "inner")
        .filter(F.col("transaction_date") == F.lit(operation_date))
        .groupBy("station_id").agg(F.count("*").alias("orders"),
            F.sum("total_elec_kwh").alias("energy_kwh"),
            F.sum(F.coalesce("total_fee", F.lit(0.0))).alias("revenue"),
            (F.sum("duration_seconds") / 60).alias("duration_min")))
    selected_meta = (eligible.join(selected_day, "station_id", "left")
        .fillna({"orders": 0, "energy_kwh": 0.0, "revenue": 0.0, "duration_min": 0.0})
        .orderBy("model_station_id"))
    quality_counts = {row.quality_reason: row["count"] for row in
                      rejected.groupBy("quality_reason").count().collect()}
    report = {
        "run_id": args.run_id,
        "source": "Figshare Beijing public charging transactions",
        "source_doi": "10.6084/m9.figshare.31952289.v2",
        "raw_records": raw_count,
        "deduplicated_records": raw_count - duplicates,
        "duplicate_records": duplicates,
        "clean_records": clean_count,
        "rejected_records": rejected_count,
        "quality_reasons": quality_counts,
        "source_station_count": station_count,
        "selected_stations": rows(selected_meta),
        "hourly_rows": len(collected),
        "split_counts": {row.split: row["count"] for row in
                         final.groupBy("split").count().collect()},
        "gbt_evaluation": evaluations,
        "gbt_model_run_id": args.reuse_model_run_id or args.run_id,
        "operation_date": operation_date.isoformat(),
        "operation": operation,
        "daily": rows(daily),
        "site_type": rows(site_type),
        "tariff": rows(tariff),
        "weekday": rows(weekday),
        "station_device_total": int(stations.select(F.sum(F.greatest(
            F.lit(1.0), F.coalesce("charging_gun_num", "piles_num", F.lit(1.0))))).first()[0]),
        "hdfs_feature_path": root + "/features/hourly/version=" + args.run_id,
        "training_rows_output": str(output),
        "application_id": spark.sparkContext.applicationId,
        "spark_master": spark.sparkContext.master,
        "period_gap_policy": "2025-01 and 2025-07 are separate contiguous panels; no zero fill across months",
        "energy_allocation_assumption": "Session kWh is distributed uniformly over its reported duration",
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    report_path = REPO / "bigdata/reports/real_beijing_spark_run.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2, default=str),
                           encoding="utf-8")
    print(json.dumps({"report": str(report_path), "rows": len(collected),
                      "stations": len(report["selected_stations"]),
                      "clean_records": clean_count}, ensure_ascii=False))
    spark.stop()


if __name__ == "__main__":
    main()
