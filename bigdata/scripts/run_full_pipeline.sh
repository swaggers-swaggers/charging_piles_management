#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo"
export RUN_ID="${RUN_ID:-$(date +%Y%m%d_%H%M%S)}"
config="${1:-bigdata/conf/dev.yaml}"
mkdir -p .bigdata/logs/"$RUN_ID"
failed() {
  "$repo/.venv-bigdata/bin/python" - "$RUN_ID" "$stage" "$config" <<'PY'
import sys,json,os,datetime
from pathlib import Path
p=Path('.bigdata/sample-dashboard/data/latest-attempt.json' if 'sample' in sys.argv[3] else 'code/web/data/latest-attempt.json');p.parent.mkdir(parents=True,exist_ok=True)
t=p.with_suffix('.tmp');t.write_text(json.dumps(dict(runId=sys.argv[1],stage=sys.argv[2],status='FAILED',generatedAt=datetime.datetime.now().astimezone().isoformat())))
os.replace(t,p)
PY
}
trap failed ERR
for stage in ingest_to_ods profile_quality clean_to_dwd build_station_load build_ads build_load_features train_global_load train_station_load build_user_matrix train_user_als evaluate_models predict_station_load export_dashboard; do
  echo "[$RUN_ID] $stage"
  "$repo/bigdata/scripts/submit.sh" "$stage" --config "$config" --run-id "$RUN_ID" > ".bigdata/logs/$RUN_ID/$stage.log" 2>&1
done
printf 'SUCCESS %s\n' "$RUN_ID"
