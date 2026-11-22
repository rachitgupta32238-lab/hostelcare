#!/usr/bin/env bash
# HostelCare live smoke test (Git Bash). Run while the backend is running.
# Usage: ./scripts/smoke_test.sh <admin-username> <admin-password>
# Read-only except for logging in/out: it does not change any data.

API=${API:-http://localhost:18080/api}
USER=${1:?admin username required}
PASS=${2:?admin password required}
fail=0

check() { # name expected actual
  if [ "$2" = "$3" ]; then echo "PASS  $1"; else echo "FAIL  $1 (expected $2, got $3)"; fail=1; fi
}
code() { curl -s -o /dev/null -w "%{http_code}" "$@"; }

check "health"                              200 "$(code $API/health)"
check "complaints need login"               401 "$(code $API/complaints)"
check "workers need login"                  401 "$(code $API/workers)"
check "wrong admin password rejected"       401 "$(code -X POST $API/admin/login -H 'Content-Type: application/json' -d '{"username":"'"$USER"'","password":"definitely-wrong-1"}')"

TOKEN=$(curl -s -X POST $API/admin/login -H "Content-Type: application/json" \
  -d '{"username":"'"$USER"'","password":"'"$PASS"'"}' | sed -n 's/.*"token":"\([0-9a-f]*\)".*/\1/p')
check "admin login returns token"           64 "${#TOKEN}"

AUTH="Authorization: Bearer $TOKEN"
check "admin reads complaints"              200 "$(code $API/complaints -H "$AUTH")"
check "admin reads workers"                 200 "$(code $API/workers -H "$AUTH")"
check "admin dashboard"                     200 "$(code $API/admin/dashboard -H "$AUTH")"
check "analytics"                           200 "$(code $API/admin/analytics -H "$AUTH")"
check "dispatch queue"                      200 "$(code $API/dispatch/queue -H "$AUTH")"
check "notifications"                       200 "$(code $API/notifications -H "$AUTH")"
check "worker-only route refuses admin"     403 "$(code $API/worker/tasks -H "$AUTH")"
check "route IT Support -> Atlantis"        true "$(curl -s $API/routes/shortest/2/6 | sed -n 's/.*"pathExists":\(true\|false\).*/\1/p')"
check "CORS preflight"                      204 "$(code -X OPTIONS $API/complaints -H 'Origin: http://localhost:5173' -H 'Access-Control-Request-Method: GET')"
check "logout"                              200 "$(code -X POST $API/auth/logout -H "$AUTH")"
check "token dead after logout"             401 "$(code $API/complaints -H "$AUTH")"

[ $fail = 0 ] && echo "All smoke tests passed." || { echo "Some smoke tests FAILED."; exit 1; }
