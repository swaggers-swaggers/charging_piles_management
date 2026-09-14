#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
export PYSPARK_PYTHON="${PYSPARK_PYTHON:-$repo/.venv-bigdata/bin/python}"
export HADOOP_CONF_DIR="${HADOOP_CONF_DIR:-${HADOOP_HOME:-$HOME/hadoop}/etc/hadoop}"
export SPARK_LOCAL_IP="${SPARK_LOCAL_IP:-127.0.0.1}"
export TZ=Asia/Shanghai
master="${SPARK_MASTER:-local[4]}"
export SPARK_SUBMIT_MASTER="$master"
spark_submit="${SPARK_SUBMIT_BIN:-$repo/.venv-bigdata/bin/spark-submit}"
if [[ ! -x "$spark_submit" ]]; then
  spark_submit="$(command -v spark-submit || true)"
fi
if [[ -z "$spark_submit" || ! -x "$spark_submit" ]]; then
  echo "spark-submit 不可用；请安装 PySpark 或设置 SPARK_SUBMIT_BIN。" >&2
  exit 1
fi
exec "$spark_submit" --master "$master" --driver-memory 4g --conf spark.executor.memory=2g --conf spark.executor.cores=2 --conf spark.executor.instances=2 --conf spark.ui.showConsoleProgress=false --py-files "$repo/bigdata/jobs/common.py,$repo/bigdata/jobs/quality.py,$repo/bigdata/jobs/build_load_features.py,$repo/bigdata/jobs/modeling.py" "$repo/bigdata/jobs/$1.py" "${@:2}"
