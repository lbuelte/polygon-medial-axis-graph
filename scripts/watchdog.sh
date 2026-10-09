#!/bin/bash
# watchdog.sh - Run medial_axis_graph with per-instance timeout and crash detection
#
# Usage (from the repository root):
#   scripts/watchdog.sh [timeout_seconds] -- <medial_axis_graph arguments>
# Example:
#   scripts/watchdog.sh 10 -- --input input --output output --no-svg
#
# Instances that make no progress for timeout_seconds (default 10) or cause a
# segmentation fault are appended to the stuck/segfault instance lists, and the
# run is restarted. The lists are passed to the tool with --perturb-instances,
# so these instances are perturbed before their medial axis is computed.
# Set EXECUTABLE to use another build of the tool.

EXECUTABLE="${EXECUTABLE:-build/medial_axis_graph}"
TIMEOUT_SECONDS=10
if [[ $# -gt 0 && "$1" != "--" ]]; then
    TIMEOUT_SECONDS="$1"
    shift
fi
if [[ "$1" != "--" ]]; then
    echo "Usage: scripts/watchdog.sh [timeout_seconds] -- <medial_axis_graph arguments>" >&2
    exit 2
fi
shift
TOOL_ARGS=("$@")

STUCK_INSTANCES_FILE="scripts/stuck_instances.txt"
SEGFAULT_INSTANCES_FILE="scripts/segfault_instances.txt"
LOG_FILE="watchdog.log"
PROGRESS_FILE="last_progress.tmp"  # the tool writes "DATASET|INDEX" of the current instance here

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# Ensure stuck instances file exists
touch "$STUCK_INSTANCES_FILE" "$SEGFAULT_INSTANCES_FILE" "$LOG_FILE" "$PROGRESS_FILE"

log "Starting watchdog. Timeout: ${TIMEOUT_SECONDS}s"

while true; do
    # Clean progress file before each run
    echo "" > "$PROGRESS_FILE"

    # Launch the C++ program in background
    "$EXECUTABLE" "${TOOL_ARGS[@]}" \
        --perturb-instances "$STUCK_INSTANCES_FILE" \
        --perturb-instances "$SEGFAULT_INSTANCES_FILE" &
    CHILD_PID=$!

    STUCK=false

    # Monitor loop: check if child is alive and if instance changed recently
    LAST_SEEN=""
    LAST_CHANGE_TIME=$(date +%s)

    while kill -0 "$CHILD_PID" 2>/dev/null; do
        sleep 1

        CURRENT=$(cat "$PROGRESS_FILE" 2>/dev/null | tr -d '[:space:]')
        NOW=$(date +%s)

        if [[ "$CURRENT" != "$LAST_SEEN" ]]; then
            LAST_SEEN="$CURRENT"
            LAST_CHANGE_TIME=$NOW
        else
            ELAPSED=$(( NOW - LAST_CHANGE_TIME ))
            if [[ $ELAPSED -ge $TIMEOUT_SECONDS && -n "$CURRENT" ]]; then
                DATASET=$(echo "$CURRENT" | cut -d'|' -f1)
                INSTANCE_ID=$(echo "$CURRENT" | cut -d'|' -f2)

                log "TIMEOUT on instance: dataset=$DATASET instance=$INSTANCE_ID (stuck for ${ELAPSED}s)"
                echo "$DATASET $INSTANCE_ID" >> "$STUCK_INSTANCES_FILE"

                kill -KILL "$CHILD_PID" 2>/dev/null
                wait "$CHILD_PID" 2>/dev/null
                STUCK=true
                break
            fi
        fi
    done

    if [[ "$STUCK" == false ]]; then
        wait "$CHILD_PID"
        EXIT_CODE=$?

        if [[ $EXIT_CODE -eq 139 ]]; then
            DATASET=$(echo "$LAST_SEEN" | cut -d'|' -f1)
            INSTANCE_ID=$(echo "$LAST_SEEN" | cut -d'|' -f2)

            log "SEGFAULT on instance: dataset=$DATASET instance=$INSTANCE_ID"
            echo "$DATASET $INSTANCE_ID" >> "$SEGFAULT_INSTANCES_FILE"

            log "Restarting after segfault..."
            sleep 1
            continue
        elif [[ $EXIT_CODE -eq 0 ]]; then
            log "Program completed successfully."
            break
        else
            log "Program exited with unexpected code $EXIT_CODE, aborting."
            break
        fi
    fi

    log "Restarting after timeout..."
    sleep 1
done

log "Watchdog done."