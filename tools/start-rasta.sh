#!/usr/bin/env bash
set -uo pipefail

# Starts (or restarts) the rasta display viewer this GEM port depends
# on to actually show anything: GEM itself only writes into a shared
# framebuffer file and sends a UDP "here's my display" datagram --
# without rasta listening on the other end, GEM runs perfectly
# normally but nothing ever becomes visible.
#
# rasta lives outside this repo; adjust RASTA_BIN if it moves.

RASTA_BIN="/home/tstih/data/tstih/rasta/bin/rasta"
PORT="${GEM_RASTA_PORT:-5000}"
LOG="/tmp/rasta_task.log"

if [ ! -x "$RASTA_BIN" ]; then
    echo "rasta viewer not found at $RASTA_BIN -- GEM will run but nothing will be visible" >&2
    exit 1
fi

pkill -x rasta >/dev/null 2>&1
sleep 0.2

# setsid, not just "& disown": VS Code tears down a completed task's
# whole process group, and disown only stops the shell from waiting
# on the job -- it does not change its group, so a plain background
# job still dies with it. setsid gives the child its own session/
# group so it survives the task exiting.
setsid "$RASTA_BIN" --port "$PORT" >"$LOG" 2>&1 </dev/null &
disown

sleep 0.3
if ! pgrep -x rasta >/dev/null 2>&1; then
    echo "rasta viewer exited immediately -- check $LOG" >&2
    cat "$LOG" >&2
    exit 1
fi

echo "rasta viewer running on UDP port $PORT"
