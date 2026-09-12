#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$HOME/.config/charging/env.sh" ]]; then
    source "$HOME/.config/charging/env.sh"
fi
export CHARGING_DB="${CHARGING_DB:-$root/database/test.db}"
export CHARGING_WEB_DIR="${CHARGING_WEB_DIR:-$root/code/web}"
cd "$root"
case "${1:-server}" in
    server) exec "$root/build/server/ChargingServer" ;;
    client) exec "$root/build/client/ChargingClient" ;;
    hadoop) exec bash "$root/scripts/hadoop-local.sh" start ;;
    dashboard) exec xdg-open http://127.0.0.1:8080 ;;
    *) echo "Usage: $0 {server|client|hadoop|dashboard}" >&2; exit 2 ;;
esac
