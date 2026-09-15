#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
run_id="${RUN_ID:-beijing_$(date +%Y%m%d_%H%M%S)}"
source_dir="$repo/.bigdata/source/real/beijing_figshare"
spark_id="${run_id}_spark"
cnn_id="${run_id}_cnn"
dashboard_id="${run_id}_hybrid"
python_cmd="${PYTHON_CMD:-python3}"
hdfs_cmd="${HDFS_CMD:-}"
spark_submit="${SPARK_SUBMIT:-}"
[[ -n "$hdfs_cmd" ]] || hdfs_cmd="$(command -v hdfs || true)"
[[ -n "$spark_submit" ]] || spark_submit="$(command -v spark-submit || true)"
if [[ -z "$hdfs_cmd" || -z "$spark_submit" ]]; then
  printf 'ERROR: hdfs and spark-submit must be available; override HDFS_CMD/SPARK_SUBMIT when needed.\n' >&2
  exit 1
fi
hadoop_root="$(cd "$(dirname "$hdfs_cmd")/.." && pwd)"
export HADOOP_CONF_DIR="${HADOOP_CONF_DIR:-$hadoop_root/etc/hadoop}"
export YARN_CONF_DIR="${YARN_CONF_DIR:-$HADOOP_CONF_DIR}"

cd "$repo"
"$python_cmd" bigdata/scripts/download_real_beijing.py --output "$source_dir"

"$hdfs_cmd" dfs -mkdir -p /charging_real/beijing/ods
"$hdfs_cmd" dfs -put -f "$source_dir/stations_public.parquet" /charging_real/beijing/ods/stations_public.parquet
"$hdfs_cmd" dfs -put -f "$source_dir/orders_2025-01_public.parquet" /charging_real/beijing/ods/orders_2025-01_public.parquet
"$hdfs_cmd" dfs -put -f "$source_dir/orders_2025-07_public.parquet" /charging_real/beijing/ods/orders_2025-07_public.parquet
"$hdfs_cmd" dfs -put -f "$source_dir/manifest.json" /charging_real/beijing/ods/manifest.json

"$spark_submit" --master yarn \
  --conf spark.sql.legacy.parquet.nanosAsLong=true \
  bigdata/jobs/prepare_real_beijing.py --run-id "$spark_id" --max-stations 20

"$python_cmd" bigdata/experiments/train_real_residual_cnn.py \
  --input .bigdata/experiments/real_beijing_load.jsonl \
  --spark-report bigdata/reports/real_beijing_spark_run.json \
  --run-id "$cnn_id" --epochs 15 --train-stride 2 \
  --hdfs-root /charging_real/beijing \
  --dataset-name "Beijing public charging transactions (Figshare 31952289 v2)" \
  --report-output bigdata/reports/real_beijing_cnn_experiment.json \
  --dashboard-output code/web/data/real-load-forecast.json

"$python_cmd" bigdata/serving/export_beijing_hybrid_dashboard.py --run-id "$dashboard_id"
printf 'SUCCESS %s\n' "$dashboard_id"
