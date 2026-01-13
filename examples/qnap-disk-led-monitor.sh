#!/bin/bash
# QNAP LED Disk Activity Monitor

################################################################################
# Systemd unit file (save as /etc/systemd/system/qnap-disk-led-monitor.service)
################################################################################

#[Unit]
#Description=QNAP Disk LED Activity Monitor
#After=local-fs.target
#
#[Service]
#Type=simple
#ExecStart=/usr/local/bin/qnap-disk-led-monitor.sh
#Restart=always
#RestartSec=10
#
#[Install]
#WantedBy=multi-user.target

################################################################################
# Configuration
# Exmaple for /etc/qnap-disk-led-monitor.conf:
# /etc/qnap-disk-led-monitor.conf - example content
#
# AUTOLOAD_QAP8528_MODULE=1
# AUTOLOAD_LEDTRIG_MODULE=1
# POLL_INTERVAL=0.5
# IDLE_TIMEOUT_MS=1500
# LED_ON_BRIGHTNESS=1
# USE_BLKTRACE=0
# m2ssd1:nvme-XXXXXXXXXXXXXX
# m2ssd2:nvme-YYYYYYYYYYYYYY
# hdd3:ata-XXXXXXXXXXXXXXX
# hdd4:ata-YYYYYYYYYYYYYY
#
################################################################################

# Config file location (can override with -c flag)
CONFIG_FILE="${CONFIG_FILE:-/etc/qnap-disk-led-monitor.conf}"

# Default settings (can be overridden in config file)
AUTOLOAD_QAP8528_MODULE=1
AUTOLOAD_LEDTRIG_MODULE=1
POLL_INTERVAL=0.5
IDLE_TIMEOUT_MS=1500
LED_ON_BRIGHTNESS=1
# Set to 1 to blink on any IO (including drivetemp), 0 for data IO only
BLINK_ON_ALL_IO=0
# Set to 1 to use blktrace instead of stat (catches SMART/drivetemp)
USE_BLKTRACE=0

# Disk mappings array - populated from config file
declare -a DISK_MAPPINGS=()

################################################################################
# Code
################################################################################

LED_BASE="/sys/class/leds"
LED_PREFIX="qnap8528::"
declare -a STAT_FILES LED_SUFFIXES PREV_TOTALS LAST_ACT_MS IS_BLINKING
declare -a BLKTRACE_PIDS BLKTRACE_FDS DISK_DEVICES

die() { echo "[ERROR] $*" >&2; exit 1; }
log() { echo "[INFO] $*"; }

load_module() {
    local mod=$1
    local mod_dir="/sys/module/${mod//-/_}"
    if [[ ! -d "$mod_dir" ]]; then
        log "Loading module: $mod"
        modprobe "$mod" 2>/dev/null || die "Failed to load $mod"
    fi
}

parse_config() {
    [[ -f "$CONFIG_FILE" ]] || die "Config not found: $CONFIG_FILE"
    
    local line key val
    while IFS= read -r line || [[ -n "$line" ]]; do
        # Skip empty/comments
        [[ -z "$line" || "$line" == \#* ]] && continue
        
        # Trim whitespace
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"
        
        # Parse KEY=VALUE settings
        if [[ "$line" == *=* && "$line" != *:* ]]; then
            key="${line%%=*}"
            val="${line#*=}"
            case "$key" in
                AUTOLOAD_QAP8528_MODULE)     AUTOLOAD_QAP8528_MODULE="$val" ;;
                AUTOLOAD_LEDTRIG_MODULE)     AUTOLOAD_LEDTRIG_MODULE="$val" ;;
                POLL_INTERVAL)     POLL_INTERVAL="$val" ;;
                IDLE_TIMEOUT_MS)   IDLE_TIMEOUT_MS="$val" ;;
                LED_ON_BRIGHTNESS) LED_ON_BRIGHTNESS="$val" ;;
                BLINK_ON_ALL_IO)   BLINK_ON_ALL_IO="$val" ;;
                USE_BLKTRACE)      USE_BLKTRACE="$val" ;;
            esac
            continue
        fi
        
        # Parse LED:DISK mappings
        if [[ "$line" == *:* ]]; then
            DISK_MAPPINGS+=("$line")
        fi
    done < "$CONFIG_FILE"
    
    [[ ${#DISK_MAPPINGS[@]} -gt 0 ]] || die "No disk mappings found in config"
}

# Time detection - fallback for kernels without fractional seconds
read -r up _ < /proc/uptime
if [[ "$up" == *.* ]]; then
    get_now_ms() { read -r up _ < /proc/uptime; now_ms=$(( ${up//.} * 10 )); }
else
    get_now_ms() { read -r up _ < /proc/uptime; now_ms=$(( up * 1000 )); }
fi

cleanup() {
    echo ""
    log "Stopping: Restoring LEDs"
    for suffix in "${LED_SUFFIXES[@]}"; do
        echo 0 > "$LED_BASE/${LED_PREFIX}$suffix/brightness" 2>/dev/null
        echo none > "$LED_BASE/${LED_PREFIX}$suffix/trigger" 2>/dev/null
        echo "$LED_ON_BRIGHTNESS" > "$LED_BASE/${LED_PREFIX}$suffix/brightness" 2>/dev/null
    done
    # Kill blktrace processes
    for pid in "${BLKTRACE_PIDS[@]}"; do
        kill "$pid" 2>/dev/null
    done
    exit 0
}

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -c) CONFIG_FILE="$2"; shift 2 ;;
            -h|--help)
                echo "Usage: $0 [-c CONFIG_FILE]"
                echo "Default config: /etc/qnap-disk-led-monitor.conf"
                exit 0
                ;;
            *) die "Unknown option: $1" ;;
        esac
    done

    # Check root
    [[ $EUID -eq 0 ]] || die "Must run as root"

    # Parse config
    parse_config

    # Load required modules
    if (( AUTOLOAD_QAP8528_MODULE )); then
        log "Auto-loading qnap8528 module"
        load_module qnap8528
    else 
        log "Skipping qnap8528 module auto-load"
    fi

    if (( AUTOLOAD_LEDTRIG_MODULE )); then
        log "Auto-loading ledtrig-timer module"
        load_module ledtrig_timer
    else 
        log "Skipping ledtrig-timer module auto-load"
    fi

   

    log "Settings: poll=${POLL_INTERVAL}s, timeout=${IDLE_TIMEOUT_MS}ms, all_io=${BLINK_ON_ALL_IO}, blktrace=${USE_BLKTRACE}"

    # Initialize mappings
    log "Initializing mappings..."
    local suffix disk_id disk_path real_dev stat_file
    for mapping in "${DISK_MAPPINGS[@]}"; do
        suffix="${mapping%%:*}"
        disk_id="${mapping#*:}"
        disk_path="/dev/disk/by-id/$disk_id"
        
        real_dev=$(basename "$(readlink -f "$disk_path")" 2>/dev/null)
        # Strip partition: sda1->sda, nvme0n1p1->nvme0n1
        if [[ "$real_dev" == nvme* ]]; then
            # NVMe: strip pN partition suffix if present
            real_dev="${real_dev%p[0-9]*}"
        else
            # SATA/SAS: strip trailing number if it's a partition
            real_dev="${real_dev%[0-9]}"
        fi
        
        stat_file="/sys/block/$real_dev/stat"
        
        if [[ -f "$stat_file" && -d "$LED_BASE/${LED_PREFIX}$suffix" ]]; then
            STAT_FILES+=("$stat_file")
            LED_SUFFIXES+=("$suffix")
            DISK_DEVICES+=("$real_dev")
            PREV_TOTALS+=(0)
            LAST_ACT_MS+=(0)
            IS_BLINKING+=(0)
            
            echo none > "$LED_BASE/${LED_PREFIX}$suffix/trigger"
            echo "$LED_ON_BRIGHTNESS" > "$LED_BASE/${LED_PREFIX}$suffix/brightness"
            
            log "Mapped $suffix -> $real_dev"
        else
            log "Warning: Skipping $suffix - stat:$([[ -f "$stat_file" ]] && echo ok || echo missing) led:$([[ -d "$LED_BASE/${LED_PREFIX}$suffix" ]] && echo ok || echo missing)"
        fi
    done

    [[ ${#STAT_FILES[@]} -eq 0 ]] && die "No valid disk mappings"

    # Start blktrace processes if enabled
    if (( USE_BLKTRACE )); then
        log "Starting blktrace for ${#DISK_DEVICES[@]} disks..."
        for i in "${!DISK_DEVICES[@]}"; do
            local fifo="/tmp/.blktrace-${DISK_DEVICES[i]}-$$"
            mkfifo "$fifo"
            blktrace -d "/dev/${DISK_DEVICES[i]}" -o - > "$fifo" 2>/dev/null &
            BLKTRACE_PIDS[i]=$!
            exec {fd}<>"$fifo"
            BLKTRACE_FDS[i]=$fd
            rm "$fifo"
            log "  blktrace for ${DISK_DEVICES[i]} (pid ${BLKTRACE_PIDS[i]}, fd $fd)"
        done
    fi

    trap cleanup SIGINT SIGTERM EXIT

    log "Monitoring ${#STAT_FILES[@]} disks..."

    # Create a fifo for sleep - read will block until timeout
    local sleep_fifo="/tmp/.disk-led-monitor-$$"
    mkfifo "$sleep_fifo"
    exec 9<>"$sleep_fifo"
    rm "$sleep_fifo"

    # Main loop - check module is still loaded by checking first LED exists
    local current_total led_path rd_ops rd_sec wr_ops wr_sec has_activity
    while [[ -d "$LED_BASE/${LED_PREFIX}${LED_SUFFIXES[0]}" ]]; do
        get_now_ms

        for i in "${!STAT_FILES[@]}"; do
            has_activity=0
            
            if (( USE_BLKTRACE )); then
                # Check if blktrace has output (non-blocking read)
                if read -t 0.001 -u "${BLKTRACE_FDS[i]}" -n 1 2>/dev/null; then
                    has_activity=1
                    # Drain any remaining data
                    while read -t 0.001 -u "${BLKTRACE_FDS[i]}" -n 1024 2>/dev/null; do :; done
                fi
            else
                # Use stat file
                read -r rd_ops _ rd_sec _ wr_ops _ wr_sec _ < "${STAT_FILES[i]}"
                
                if (( BLINK_ON_ALL_IO )); then
                    current_total=$(( rd_ops + rd_sec + wr_ops + wr_sec ))
                else
                    current_total=$(( rd_sec + wr_sec ))
                fi
                
                if (( current_total != PREV_TOTALS[i] )); then
                    PREV_TOTALS[i]=$current_total
                    has_activity=1
                fi
            fi
            
            led_path="$LED_BASE/${LED_PREFIX}${LED_SUFFIXES[i]}"

            if (( has_activity )); then
                # Activity detected
                LAST_ACT_MS[i]=$now_ms

                if (( IS_BLINKING[i] == 0 )); then
                    #echo "[DEBUG] i=$i START BLINK"
                    if ! echo timer > "$led_path/trigger" 2>/dev/null; then
                        die "'timer' trigger unavailable - ledtrig-timer removed?"
                    fi
                    IS_BLINKING[i]=1
                fi
            elif (( IS_BLINKING[i] == 1 )); then
                # Check timeout
                local elapsed=$(( now_ms - LAST_ACT_MS[i] ))
                #echo "[DEBUG] i=$i IDLE: elapsed=${elapsed}ms timeout=${IDLE_TIMEOUT_MS}ms blink=${IS_BLINKING[i]}"
                if (( elapsed > IDLE_TIMEOUT_MS )); then
                    #echo "[DEBUG] i=$i STOP BLINK: writing to $led_path"
                    # Timeout - stop blinking
                    echo 0 > "$led_path/brightness"
                    echo none > "$led_path/trigger"
                    echo "$LED_ON_BRIGHTNESS" > "$led_path/brightness"
                    IS_BLINKING[i]=0
                    #echo "[DEBUG] i=$i AFTER STOP: blink=${IS_BLINKING[i]}"
                fi
            fi
        done

        # Sleep using read timeout on fifo (blocks until timeout)
        read -t "$POLL_INTERVAL" -u 9 2>/dev/null || true
    done

    die "Hardware driver removed"
}

main "$@"
