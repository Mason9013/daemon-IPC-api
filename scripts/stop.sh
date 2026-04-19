#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RUNTIME_DIR="$PROJECT_DIR/server_runtime"
PID_FILE="$RUNTIME_DIR/daemon.pid"

echo "Stopping daemon..."

if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    kill "$PID" 2>/dev/null && echo "Killed daemon PID $PID" || echo "Daemon PID $PID was not running"
    rm -f "$PID_FILE"
else
    echo "No PID file found — killing by name..."
    pkill -f "daemon server_runtime" 2>/dev/null || true
fi

echo "Cleaning runtime..."
pkill -f "demo_worker" 2>/dev/null || true
rm -f "$RUNTIME_DIR"/web_*.0
rm -f "$RUNTIME_DIR"/web_*.1
rm -f "$RUNTIME_DIR"/REQUESTS

echo "Done. Runtime is clean."
