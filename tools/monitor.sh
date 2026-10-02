#!/usr/bin/env bash

PID="${1:-}"
PORT="${2:-8080}"

if [[ -z "$PID" ]]; then
    PID="$(pgrep -n -x server || true)"
fi

if [[ ! "$PID" =~ ^[0-9]+$ ]] || [[ ! -d "/proc/$PID/fd" ]]; then
    printf 'Usage: %s [server-pid] [port]\n' "$0" >&2
    exit 1
fi

if [[ ! "$PORT" =~ ^[0-9]+$ ]] || (( PORT < 1 || PORT > 65535 )); then
    printf 'Port must be a number from 1 to 65535.\n' >&2
    exit 1
fi

read -r CPU RSS_KB < <(ps -p "$PID" -o %cpu=,rss=)
MEMORY_MB="$(awk -v rss="$RSS_KB" 'BEGIN { printf "%.1f MB", rss / 1024 }')"
CONNECTIONS="$(ss -Htn state established "sport = :$PORT" | wc -l | tr -d ' ' )"
OPEN_FDS="$(find "/proc/$PID/fd" -mindepth 1 -maxdepth 1 -type l | wc -l | tr -d ' ' )"

printf '=== Mini TCP Server Monitor ===\n\n'
printf 'PID:          %s\n' "$PID"
printf 'CPU:          %s %%\n' "${CPU:-0}"
printf 'Memory:       %s\n' "$MEMORY_MB"
printf 'Connections:  %s\n' "$CONNECTIONS"
printf 'Open FDs:     %s\n' "$OPEN_FDS"