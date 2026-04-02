#!/bin/bash

# APRS TX Log Analyzer
# Analyzes captured TX data for common transmission issues

LOG_FILE="${1:-.}"

if [ -d "$LOG_FILE" ]; then
    # Find most recent log file
    LOG_FILE=$(find "$LOG_FILE" -name "*aprs_tx_diagnostic*.log" -o -name "tx_*.log" | sort -r | head -1)
    
    if [ -z "$LOG_FILE" ]; then
        echo "No diagnostic log files found!"
        exit 1
    fi
fi

if [ ! -f "$LOG_FILE" ]; then
    echo "Log file not found: $LOG_FILE"
    exit 1
fi

echo "Analyzing: $LOG_FILE"
echo ""
echo "═══════════════════════════════════════════════════════"
echo "APRS TX ANALYSIS REPORT"
echo "═══════════════════════════════════════════════════════"
echo ""

# Analysis functions
count_tx_events() {
    echo "TX Events Found:"
    grep -i "tx\|transmit\|send" "$LOG_FILE" | wc -l || echo "0"
}

find_frequency() {
    echo "Frequency References:"
    grep -i "432\|frequency\|freq" "$LOG_FILE" | head -5 || echo "None found"
}

check_packet_size() {
    echo "Packet Size Information:"
    grep -i "byte\|size\|length\|packet" "$LOG_FILE" | head -10 || echo "No size info found"
}

analyze_timing() {
    echo ""
    echo "Transmission Timeline:"
    grep -oP '\[\d{2}:\d{2}:\d{2}\.\d{3}\]' "$LOG_FILE" | uniq || echo "No timing info"
}

count_tx_events
echo ""
find_frequency
echo ""
check_packet_size
analyze_timing

echo ""
echo "═══════════════════════════════════════════════════════"
echo ""
