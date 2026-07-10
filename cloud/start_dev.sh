#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# Lock-In K-means local dev — starts all three services
#
#   Service 1 — Pipeline function server  →  http://localhost:8080
#   Service 2 — Flask dashboard           →  http://localhost:5000
#   Service 3 — Daily scheduler           →  triggers pipeline daily at 00:00
#
# Usage:
#   ./start_dev.sh             # start everything
#   ./start_dev.sh --no-sched  # skip scheduler (manual trigger via dashboard)
# ──────────────────────────────────────────────────────────────────────────────

set -euo pipefail
cd "$(dirname "$0")"

NO_SCHED=false
[[ "${1:-}" == "--no-sched" ]] && NO_SCHED=true

# ── Colors ──
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RESET='\033[0m'

echo -e "${GREEN}Starting Lock-In K-means local dev environment…${RESET}"
echo ""

# ── 1. Pipeline function server ──
echo -e "${YELLOW}[1/3] Pipeline function server → :8080${RESET}"
cd functions
functions-framework --target=run_kmeans --port=8080 &
FUNC_PID=$!
cd ..

sleep 1   # give it a moment to bind

# ── 2. Dashboard ──
echo -e "${YELLOW}[2/3] Dashboard → :5000${RESET}"
cd dashboard
python3 app.py &
DASH_PID=$!
cd ..

# ── 3. Scheduler ──
if [ "$NO_SCHED" = false ]; then
  echo -e "${YELLOW}[3/3] Daily scheduler (also fires once now)${RESET}"
  cd scheduler
  python3 run_scheduler.py &
  SCHED_PID=$!
  cd ..
else
  echo -e "${YELLOW}[3/3] Scheduler skipped (--no-sched)${RESET}"
  SCHED_PID=""
fi

echo ""
echo -e "${GREEN}  Pipeline:  http://localhost:8080${RESET}"
echo -e "${GREEN}  Dashboard: http://localhost:5000${RESET}"
echo ""
echo "Press Ctrl+C to stop all services"

cleanup() {
  echo ""
  echo "Stopping services…"
  kill $FUNC_PID $DASH_PID ${SCHED_PID:-} 2>/dev/null || true
}
trap cleanup EXIT INT TERM

wait $FUNC_PID $DASH_PID ${SCHED_PID:-}
