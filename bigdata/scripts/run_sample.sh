#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo"
# A sample must not overwrite the full lake or public dashboard.
unset BIGDATA_ROOT
export RUN_ID="${RUN_ID:-$(date +%Y%m%d)_sample}"
"$repo/.venv-bigdata/bin/python" bigdata/scripts/make_sample.py
bash bigdata/scripts/run_full_pipeline.sh bigdata/conf/sample.yaml
