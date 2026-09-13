#!/usr/bin/env bash
# Six bounded YARN applications, each sequentially training 7–8 actual source station IDs.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
export RUN_ID="${RUN_ID:-$(date +%Y%m%d_%H%M%S)}"
export SPARK_MASTER=yarn
mkdir -p "$repo/.bigdata/batches/$RUN_ID"
"$repo/.venv-bigdata/bin/python" - "$repo" "$RUN_ID" <<'PY'
import csv,zipfile,io,sys
from pathlib import Path
p=Path(sys.argv[1]);z=zipfile.ZipFile(p/'05.北京模拟充电数据集.zip')
n=next(x for x in z.namelist() if x.endswith('/beijing_stations.csv'))
ids=sorted(int(x['stationId']) for x in csv.DictReader(io.TextIOWrapper(z.open(n),encoding='utf-8-sig')))
for i in range(6):
 values=ids[i*len(ids)//6:(i+1)*len(ids)//6]
 (p/'.bigdata/batches'/sys.argv[2]/f'{i}.txt').write_text(','.join(map(str,values)))
PY
# Default one application at a time suits 8 GB pseudo-distributed nodes.
# Increase MAX_CONCURRENT to 2/3 only after sizing cluster resources.
pids=()
for batch in "$repo/.bigdata/batches/$RUN_ID/"*.txt; do
  "$repo/bigdata/scripts/submit.sh" train_station_load --config "${1:-$repo/bigdata/conf/cluster.yaml}" --run-id "$RUN_ID" --stations "$(cat "$batch")" > "${batch%.txt}.log" 2>&1 &
  pids+=("$!")
  if (( ${#pids[@]} >= ${MAX_CONCURRENT:-1} )); then
    for pid in "${pids[@]}"; do wait "$pid"; done
    pids=()
  fi
done
for pid in "${pids[@]}"; do wait "$pid"; done
