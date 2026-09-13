#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
hdfs="${HADOOP_HOME:-$HOME/hadoop}/bin/hdfs"
"$hdfs" dfsadmin -safemode wait
"$repo/bigdata/scripts/submit.sh" ingest_to_ods --config "${1:-$repo/bigdata/conf/cluster.yaml}" "${@:2}"
"$hdfs" dfs -test -e /charging/ods
"$hdfs" dfs -ls /charging/ods/beijing
