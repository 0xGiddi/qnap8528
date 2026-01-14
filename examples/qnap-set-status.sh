#!/bin/bash
# qnap-set-status.sh
# Logic: One state per caller ID. Highest global priority wins.
#
# Usage:
#   qnap-set-status.sh <state> [caller_id]
#
# States:
#   error      (Persistent, Red Solid)
#   warning    (Persistent, Red Blink)
#   transition (Volatile, Bicolor or Green Blink fallback)
#   activity   (Volatile, Green Blink)
#   ok         (Removes caller state, defaults to Green Solid if no other states exist)
#   clear      (Resets EVERYTHING, wipes all files, turns LED OFF)
#
# caller_id is not required, but this will allow other scripts that do not
# set a caller ID to override the state. If a caller ID is provided, it requires
# that the same caller ID will be provided next time the state is set. This allows
# Multiple callers to set their own states independently and let the highest priority
# state take effect.

set -e

DIR_RAM="/run/qnap-led"          # Volatile states (clears on reboot)
DIR_DISK="/var/lib/qnap-led"     # Persistent states (survives reboot)
LOCK_FILE="/var/lock/qnap-led.lock"
LED_BASE="/sys/class/leds/qnap8528::status"

mkdir -p "$DIR_RAM" "$DIR_DISK"

load_module() {
    local mod=$1
    local mod_dir="/sys/module/${mod//-/_}"
    if [[ ! -d "$mod_dir" ]]; then
        modprobe "$mod" 2>/dev/null || true
    fi
}

set_hw() {
    # Ensure timer trigger is available
    load_module "ledtrig_timer"

    # Reset triggers to ensure clean state application
    echo "none" > "$LED_BASE/trigger"

    case "$1" in
        error)      # Priority 5: Red Solid
            echo 2 > "$LED_BASE/brightness" ;;
            
        warning)    # Priority 4: Red Blink
            echo 2 > "$LED_BASE/brightness"
            echo "timer" > "$LED_BASE/trigger" ;;
            
        transition) # Priority 3: Bicolor (Red/Green)
            if [ -w "$LED_BASE/blink_bicolor" ]; then
                echo 1 > "$LED_BASE/blink_bicolor"
            else
                # Fallback: Green Blink
                echo 1 > "$LED_BASE/brightness"
                echo "timer" > "$LED_BASE/trigger" 
            fi
            ;;
        activity)   # Priority 2: Green Blink
            echo 1 > "$LED_BASE/brightness"
            echo "timer" > "$LED_BASE/trigger" ;;
            
        ok)         # Priority 1: Green Solid
            echo 1 > "$LED_BASE/brightness" ;;
            
        clear)      # Reset: LED Off
            echo 0 > "$LED_BASE/brightness" ;;
    esac
}

(
    flock -x 200

    CMD=$1
    CALLER=${2:-default}

    if [ -z "$CMD" ]; then
        echo "Usage: $0 <state> [caller_id]"
        exit 1
    fi

    # Remove the specific caller's previous state first
    rm -f "$DIR_RAM"/*."$CALLER" "$DIR_DISK"/*."$CALLER"

    # Register new state
    case "$CMD" in
        error)      touch "$DIR_DISK/error.$CALLER" ;;
        warning)    touch "$DIR_DISK/warning.$CALLER" ;;
        transition) touch "$DIR_RAM/transition.$CALLER" ;;
        activity)   touch "$DIR_RAM/activity.$CALLER" ;;
        ok)         ;;
        clear)      
            # Hard Reset: Wipe everything from everyone
            rm -f "$DIR_RAM"/* "$DIR_DISK"/* 
            ;; 
    esac

    # Apply new state based on priority
    if ls "$DIR_DISK"/error.* >/dev/null 2>&1; then
        set_hw "error"
    elif ls "$DIR_DISK"/warning.* >/dev/null 2>&1; then
        set_hw "warning"
    elif ls "$DIR_RAM"/transition.* >/dev/null 2>&1; then
        set_hw "transition"
    elif ls "$DIR_RAM"/activity.* >/dev/null 2>&1; then
        set_hw "activity"
    elif [ "$CMD" == "clear" ]; then
        set_hw "clear"
    else
        # If no state exists, set to OK
        set_hw "ok"
    fi

) 200>"$LOCK_FILE"
