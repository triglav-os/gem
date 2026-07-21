#!/usr/bin/env bash
set -uo pipefail

# Kills any previous gemd, starts a fresh one detached from this
# script's process, and waits for its Unix socket to appear before
# returning -- so the VS Code preLaunchTask that runs this only
# completes once gemd is actually ready to accept RPC connections.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEMD="$ROOT/bin/gemd"
SOCK="/tmp/gemd.sock"
LOG="/tmp/gemd_task.log"

pkill -x gemd >/dev/null 2>&1
sleep 0.2
rm -f "$SOCK"

# setsid, not just "& disown": VS Code tears down a completed task's
# whole process group, and disown only stops the shell from waiting
# on the job -- it does not change its group, so a plain background
# job still dies with it. setsid gives the child its own session/
# group so it survives the task exiting.
setsid "$GEMD" >"$LOG" 2>&1 </dev/null &
disown

for _ in $(seq 1 50); do
    if [ -S "$SOCK" ]; then
        echo "gemd ready at $SOCK"
        exit 0
    fi
    sleep 0.1
done

echo "gemd did not start in time" >&2
cat "$LOG" >&2
exit 1
