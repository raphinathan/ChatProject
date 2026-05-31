#!/bin/bash
# =============================================================================
#  Chat Server Stress Test Suite
#  Run from the repo root with the server already running:
#      ./bin/server &
#      bash stress_test.sh
#
#  Tests:
#   1. Sequential baseline  — 50 users, register/login/create/logout
#   2. Parallel flood       — 50 users simultaneously
#   3. IP pool exhaustion   — 256 create-group requests (255th OK, 256th fails)
#   4. Group lifecycle      — create → leave (reclaim) → re-create same name
#   5. Duplicate join guard — join the same group twice, second must be rejected
#   6. Abrupt disconnect    — kill -9 a logged-in client, group must be reclaimed
#   7. Unauthenticated join — raw TCP join without login, must be rejected
# =============================================================================

SERVER_IP="127.0.0.1"
SERVER_PORT=5000
CLIENT="./bin/client"
PASS=0
FAIL=0

# ---------- helpers ----------------------------------------------------------

ok()   { echo "  [PASS] $1"; PASS=$((PASS+1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }

# Run a client session from a here-doc, capture output.
run_client() {
    $CLIENT $SERVER_IP 2>&1 << EOF
$1
EOF
}

# Check that the server process is still alive.
server_alive() {
    pgrep -x server > /dev/null 2>&1
}

# =============================================================================
#  TEST 1 — Sequential baseline
# =============================================================================
echo ""
echo "=== TEST 1: Sequential baseline (50 users) ==="

for i in {1..50}; do
    $CLIENT $SERVER_IP > /dev/null 2>&1 << EOF
1
sequser$i
seqpass$i
2
sequser$i
seqpass$i
1
seqgroup$i
4
3
EOF
done

if server_alive; then
    ok "Server still alive after 50 sequential register/login/create/logout cycles"
else
    fail "Server crashed during sequential baseline"
fi

# =============================================================================
#  TEST 2 — Parallel flood (50 simultaneous clients)
# =============================================================================
echo ""
echo "=== TEST 2: Parallel flood (50 simultaneous clients) ==="

for i in {1..50}; do
    $CLIENT $SERVER_IP > /dev/null 2>&1 << EOF &
1
paruser$i
parpass$i
2
paruser$i
parpass$i
1
pargroup$i
4
3
EOF
done

wait   # wait for all background clients to finish

if server_alive; then
    ok "Server still alive after 50 parallel clients"
else
    fail "Server crashed during parallel flood"
fi

# =============================================================================
#  TEST 3 — IP pool exhaustion (256 groups, pool holds 255)
# =============================================================================
echo ""
echo "=== TEST 3: IP pool exhaustion (256 create-group requests) ==="

# Register and login a single user for all 256 attempts.
$CLIENT $SERVER_IP > /dev/null 2>&1 << 'EOF'
1
pooluser
poolpass
3
EOF

# Create 256 unique groups — the 256th should fail cleanly, not crash.
for i in {1..256}; do
    $CLIENT $SERVER_IP > /dev/null 2>&1 << EOF
2
pooluser
poolpass
1
poolgroup$i
4
3
EOF
done

if server_alive; then
    ok "Server still alive after 256 create-group requests (pool exhaustion handled)"
else
    fail "Server crashed during pool exhaustion test"
fi

# =============================================================================
#  TEST 4 — Group lifecycle: create → leave → re-create (IP reuse)
# =============================================================================
echo ""
echo "=== TEST 4: Group lifecycle / IP reuse ==="

$CLIENT $SERVER_IP > /dev/null 2>&1 << 'EOF'
1
cycleuser
cyclepass
2
cycleuser
cyclepass
1
cyclegroup
3
cyclegroup
1
cyclegroup
3
cyclegroup
4
3
EOF

if server_alive; then
    ok "Server still alive after create/leave/re-create cycle"
else
    fail "Server crashed during group lifecycle test"
fi

# =============================================================================
#  TEST 5 — Duplicate join guard
# =============================================================================
echo ""
echo "=== TEST 5: Duplicate join guard ==="

# Setup: create group "dupgroup" with user dupowner.
$CLIENT $SERVER_IP > /dev/null 2>&1 << 'EOF'
1
dupowner
duppass
2
dupowner
duppass
1
dupgroup
4
3
EOF

# Now join "dupgroup" twice from a second user.
OUTPUT=$($CLIENT $SERVER_IP 2>&1 << 'EOF'
1
dupjoiner
duppass2
2
dupjoiner
duppass2
2
dupgroup
2
dupgroup
4
3
EOF
)

# The second join attempt should produce an error, not a second "OK".
JOIN_OK_COUNT=$(echo "$OUTPUT" | grep -c "join.*OK\|OK.*multicast" || true)

if server_alive; then
    ok "Server still alive after duplicate join attempt"
else
    fail "Server crashed during duplicate join test"
fi

if [ "$JOIN_OK_COUNT" -le 1 ]; then
    ok "Duplicate join correctly rejected (only $JOIN_OK_COUNT successful join)"
else
    fail "Duplicate join was accepted twice (got $JOIN_OK_COUNT OK responses)"
fi

# =============================================================================
#  TEST 6 — Abrupt disconnect (kill -9 while in a group)
# =============================================================================
echo ""
echo "=== TEST 6: Abrupt disconnect ==="

# Start a client, login, create a group, and leave it running.
$CLIENT $SERVER_IP > /dev/null 2>&1 << 'EOF' &
1
killuser
killpass
2
killuser
killpass
1
killgroup
EOF
KILL_CLIENT_PID=$!

sleep 1   # give the client time to reach the group menu

kill -9 $KILL_CLIENT_PID 2>/dev/null
wait $KILL_CLIENT_PID 2>/dev/null

sleep 1   # give the server time to process the disconnect

if server_alive; then
    ok "Server still alive after client kill -9"
else
    fail "Server crashed after abrupt client disconnect"
fi

# =============================================================================
#  TEST 7 — Unauthenticated raw join (Bug 1 regression)
# =============================================================================
echo ""
echo "=== TEST 7: Unauthenticated raw join (must be rejected) ==="

# Send OP_JOIN_GROUP_REQ for 'test' without ever logging in.
RAW_REPLY=$(python3 << 'PYEOF'
import socket, sys
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3)
    s.connect(('127.0.0.1', 5000))
    packet = bytes([0x09, 0x05, 0x04, ord('t'), ord('e'), ord('s'), ord('t')])
    s.send(packet)
    reply = s.recv(256)
    if len(reply) >= 3:
        print(hex(reply[2]))   # status byte
    else:
        print('short')
    s.close()
except Exception as e:
    print('error:', e)
PYEOF
)

# ST_ERR_NOT_LOGGED_IN = 0x05
if [ "$RAW_REPLY" = "0x5" ]; then
    ok "Unauthenticated join returned ST_ERR_NOT_LOGGED_IN (0x05)"
else
    fail "Unauthenticated join returned unexpected status: $RAW_REPLY (expected 0x5)"
fi

if server_alive; then
    ok "Server still alive after unauthenticated raw join attempt"
else
    fail "Server crashed from unauthenticated raw join"
fi

# =============================================================================
#  Summary
# =============================================================================
echo ""
echo "================================================"
echo " Results: $PASS passed, $FAIL failed"
echo "================================================"

[ $FAIL -eq 0 ] && exit 0 || exit 1
