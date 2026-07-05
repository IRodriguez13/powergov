#!/usr/bin/env bash
# Compare battery sessions: powergov max-battery vs TLP (or baseline).
# Requires: battery power, fixed brightness, optional stress-ng.
set -euo pipefail

DURATION="${PG_BENCH_DURATION:-1200}"
INTERVAL="${PG_BENCH_INTERVAL:-30}"
OUT="${PG_BENCH_OUT:-/tmp/powergov-bench-$(date +%Y%m%d-%H%M%S).md}"

usage()
{
    cat <<EOF
Usage: $0 <powergov|tlp|baseline> [label]

  powergov  — TLP off, powergov on, mode max-battery
  tlp       — powergov off, TLP on
  baseline  — both off (measure only)

Env: PG_BENCH_DURATION (seconds, default 1200), PG_BENCH_INTERVAL (default 30)

Writes markdown report to PG_BENCH_OUT.
EOF
}

read_battery_pct()
{
    local cap
    for cap in /sys/class/power_supply/*/capacity; do
        [[ -f "$cap" ]] || continue
        local dir type
        dir="$(dirname "$cap")"
        type="$(cat "$dir/type" 2>/dev/null || true)"
        [[ "$type" == "Battery" ]] || continue
        cat "$cap"
        return 0
    done
    return 1
}

read_power_source()
{
    local st
    for st in /sys/class/power_supply/*/status; do
        [[ -f "$st" ]] || continue
        local dir type
        dir="$(dirname "$st")"
        type="$(cat "$dir/type" 2>/dev/null || true)"
        [[ "$type" == "Battery" ]] || continue
        cat "$st"
        return 0
    done
    echo "Unknown"
}

sample_rapl()
{
    if [[ -f /run/powergov/metrics ]]; then
        grep -i 'rapl_watts_est' /run/powergov/metrics 2>/dev/null | tail -1 || true
    fi
}

prepare_mode()
{
    local mode=$1

    case "$mode" in
    powergov)
        sudo systemctl stop tlp.service 2>/dev/null || true
        sudo systemctl start powergov.service 2>/dev/null || sudo powergov on &
        sleep 2
        sudo powergov mode max-battery
        ;;
    tlp)
        sudo systemctl stop powergov.service 2>/dev/null || true
        sudo pkill -x powergov 2>/dev/null || true
        sudo systemctl start tlp.service 2>/dev/null || true
        ;;
    baseline)
        sudo systemctl stop powergov.service 2>/dev/null || true
        sudo systemctl stop tlp.service 2>/dev/null || true
        sudo pkill -x powergov 2>/dev/null || true
        ;;
    *)
        echo "unknown mode: $mode" >&2
        exit 1
        ;;
    esac
}

run_session()
{
    local mode=$1
    local label=$2
    local start_pct end_pct start_ts end_ts
    local samples=0
    local rapl_sum=0

    if [[ "$(read_power_source)" != "Discharging" ]]; then
        echo "warning: not on battery (status=$(read_power_source))" >&2
    fi

    start_pct="$(read_battery_pct)" || { echo "no battery capacity" >&2; exit 1; }
    start_ts=$(date +%s)

    {
        echo "# powergov bench — $label"
        echo
        echo "- Mode: **$mode**"
        echo "- Start: $(date -Iseconds)"
        echo "- Duration: ${DURATION}s"
        echo "- Start SOC: ${start_pct}%"
        echo
        echo "| elapsed_s | soc_pct | note |"
        echo "|-----------|---------|------|"
    } >"$OUT"

    prepare_mode "$mode"

    local elapsed=0
    while [[ "$elapsed" -lt "$DURATION" ]]; do
        local pct note=""
        pct="$(read_battery_pct)" || pct="?"
        note="$(sample_rapl)"
        echo "| $elapsed | $pct | ${note:-} |" >>"$OUT"
        if [[ "$pct" =~ ^[0-9]+$ ]]; then
            samples=$((samples + 1))
        fi
        sleep "$INTERVAL"
        elapsed=$((elapsed + INTERVAL))
    done

    end_pct="$(read_battery_pct)" || end_pct="?"
    end_ts=$(date +%s)
    local delta=$((end_ts - start_ts))
    local soc_delta="n/a"
    if [[ "$start_pct" =~ ^[0-9]+$ && "$end_pct" =~ ^[0-9]+$ ]]; then
        soc_delta=$((start_pct - end_pct))
    fi

    {
        echo
        echo "## Summary"
        echo
        echo "- End SOC: ${end_pct}%"
        echo "- Elapsed: ${delta}s"
        echo "- SOC delta: ${soc_delta}%"
        echo "- Samples: $samples"
        echo
        echo "Repeat each mode ≥2 times at same brightness/workload before comparing."
    } >>"$OUT"

    echo "Report: $OUT"
}

main()
{
    [[ $# -ge 1 ]] || { usage; exit 1; }
    local mode=$1
    local label=${2:-$mode}
    run_session "$mode" "$label"
}

main "$@"
