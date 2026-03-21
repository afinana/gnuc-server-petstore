#!/bin/bash
# benchmark.sh — Lightweight HTTP performance test using curl
# Usage: ./benchmark.sh [REQUESTS] [CONCURRENCY]

set -euo pipefail

BASE_URL="http://127.0.0.1:8080"
TOTAL_REQUESTS=${1:-500}
CONCURRENCY=${2:-10}

echo "================================================"
echo " Petstore API Performance Benchmark"
echo "================================================"
echo " Base URL   : $BASE_URL"
echo " Requests   : $TOTAL_REQUESTS"
echo " Concurrency: $CONCURRENCY"
echo "================================================"
echo ""

run_bench() {
    local label="$1"
    local method="$2"
    local url="$3"
    local data="${4:-}"
    local reqs="$TOTAL_REQUESTS"
    local conc="$CONCURRENCY"

    local tmpdir
    tmpdir=$(mktemp -d)

    local start_ns
    start_ns=$(date +%s%N)

    local sent=0
    while [ "$sent" -lt "$reqs" ]; do
        local pids=()
        for _ in $(seq 1 "$conc"); do
            if [ "$sent" -ge "$reqs" ]; then break; fi
            sent=$((sent + 1))
            local outfile="$tmpdir/${sent}.txt"
            if [ "$method" = "GET" ] || [ "$method" = "DELETE" ]; then
                (curl -s -o /dev/null -w '%{http_code}\n%{time_total}\n' -X "$method" "$url" > "$outfile" 2>&1) &
            else
                (curl -s -o /dev/null -w '%{http_code}\n%{time_total}\n' -X "$method" \
                    -H "Content-Type: application/json" -d "$data" "$url" > "$outfile" 2>&1) &
            fi
            pids+=($!)
        done
        wait "${pids[@]}" 2>/dev/null || true
    done

    local end_ns
    end_ns=$(date +%s%N)
    local wall_ms=$(( (end_ns - start_ns) / 1000000 ))

    # Parse results
    local ok=0 fail=0 total_ms=0 min_ms=999999 max_ms=0
    for f in "$tmpdir"/*.txt; do
        [ -f "$f" ] || continue
        local code time_s time_ms
        code=$(sed -n '1p' "$f")
        time_s=$(sed -n '2p' "$f")
        [ -z "$code" ] && continue
        time_ms=$(awk "BEGIN{printf \"%d\", ${time_s:-0}*1000}")

        if [ "${code:0:1}" = "2" ]; then
            ok=$((ok + 1))
        else
            fail=$((fail + 1))
        fi
        total_ms=$((total_ms + time_ms))
        [ "$time_ms" -lt "$min_ms" ] && min_ms=$time_ms
        [ "$time_ms" -gt "$max_ms" ] && max_ms=$time_ms
    done

    local avg_ms=0
    [ "$ok" -gt 0 ] && avg_ms=$((total_ms / ok))
    [ "$min_ms" -eq 999999 ] && min_ms=0

    local rps
    rps=$(awk "BEGIN{printf \"%.1f\", ($ok + $fail) / ($wall_ms/1000)}")
    local wall_s
    wall_s=$(awk "BEGIN{printf \"%.2f\", $wall_ms/1000}")

    printf "%-30s | %4d OK | %3d ERR | avg=%4dms min=%3dms max=%5dms | %8s req/s | %ss\n" \
        "$label" "$ok" "$fail" "$avg_ms" "$min_ms" "$max_ms" "$rps" "$wall_s"

    rm -rf "$tmpdir"
}

echo "--- Results ($TOTAL_REQUESTS requests, $CONCURRENCY concurrent) ---"
echo ""

# POST create (uses a unique-ish id per run to avoid dup key errors)
run_bench "POST /v2/pet (create)" "POST" "$BASE_URL/v2/pet" \
    '{"id":99901,"name":"benchpet","status":"available","tags":[{"id":1,"name":"bench"}]}'

# GET by ID
run_bench "GET /v2/pet/{id}" "GET" "$BASE_URL/v2/pet/1"

# GET findByStatus
run_bench "GET findByStatus" "GET" "$BASE_URL/v2/pet/findByStatus?status=available"

# GET findByTags
run_bench "GET findByTags" "GET" "$BASE_URL/v2/pet/findByTags?tags=friendly"

# PUT update
run_bench "PUT /v2/pet (update)" "PUT" "$BASE_URL/v2/pet" \
    '{"id":1,"name":"updated-pet","status":"sold"}'

# DELETE
run_bench "DELETE /v2/pet/{id}" "DELETE" "$BASE_URL/v2/pet/99901"

# GET user
run_bench "GET /v2/user/{username}" "GET" "$BASE_URL/v2/user/user1"

echo ""
echo "================================================"
echo " Benchmark complete"
echo "================================================"
