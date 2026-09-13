#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
exec "$repo/bigdata/scripts/submit.sh" export_dashboard "$@"
