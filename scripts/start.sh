#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RUNTIME_DIR="$PROJECT_DIR/server_runtime"
DAEMON="$PROJECT_DIR/daemon"
WORKER="$PROJECT_DIR/demo_worker"

# Ensure runtime dir exists
mkdir -p "$RUNTIME_DIR"

# Check if already running
if [ -f "$RUNTIME_DIR/daemon.pid" ]; then
    PID=$(cat "$RUNTIME_DIR/daemon.pid")
    if kill -0 "$PID" 2>/dev/null; then
        echo "Daemon already running as PID $PID"
        exit 0
    else
        echo "Stale PID file found, cleaning up..."
        rm -f "$RUNTIME_DIR/daemon.pid"
    fi
fi

echo "Starting daemon..."
"$DAEMON" "$RUNTIME_DIR" "$WORKER" &
sleep 0.1
echo "Daemon started"
