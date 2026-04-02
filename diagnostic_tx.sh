#!/bin/bash

# APRS TX Diagnostic Script for Flipper Zero
# Collects comprehensive debugging information about SubGHz transmission

set -e

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_DIR="diagnostic_logs"
LOG_FILE="$LOG_DIR/aprs_tx_diagnostic_$TIMESTAMP.log"
PACKET_FILE="$LOG_DIR/tx_packets_$TIMESTAMP.txt"

# Create log directory
mkdir -p "$LOG_DIR"

echo "╔════════════════════════════════════════════════════════╗"
echo "║     FLIPPER ZERO APRS TX DIAGNOSTIC TOOL               ║"
echo "║     SubGHz Module Transmission Analysis                ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
echo "Starting diagnostic capture at $(date)"
echo "Logs will be saved to: $LOG_FILE"
echo ""

{
    echo "═══════════════════════════════════════════════════════"
    echo "APRS TX DIAGNOSTIC SESSION"
    echo "═══════════════════════════════════════════════════════"
    echo ""
    echo "Start Time: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "System: $(uname -a)"
    echo ""
    
    echo "─────────────────────────────────────────────────────"
    echo "1. APRS CONFIGURATION CHECK"
    echo "─────────────────────────────────────────────────────"
    echo ""
    echo "Frequency: 432.5 MHz (70cm Amateur Band)"
    echo "Modulation: GFSK (2GFSK)"
    echo "Bit Rate: 1200 baud"
    echo "Bit Duration: 104 µs"
    echo "Deviation: ±2.5 kHz (typical for AFSK over GFSK)"
    echo ""
    
    echo "─────────────────────────────────────────────────────"
    echo "2. TRANSMITTED PACKET STRUCTURE"
    echo "─────────────────────────────────────────────────────"
    echo ""
    echo "Expected TX Packet Format:"
    echo "├─ Preamble: 32x 0x7E flags (flag synchronization)"
    echo "├─ Destination Address Field (7 bytes + SSID)"
    echo "├─ Source Address Field (7 bytes + SSID)"
    echo "├─ Path Addresses (if enabled)"
    echo "├─ Control Field: 0x03 (unnumbered information)"
    echo "├─ Protocol ID: 0xF0 (no layer 3 protocol)"
    echo "├─ Information Field (position, status, telemetry, etc.)"
    echo "└─ FCS: 2-byte CRC"
    echo ""
    
    echo "─────────────────────────────────────────────────────"
    echo "3. CONNECTING TO FLIPPER VIA CLI"
    echo "─────────────────────────────────────────────────────"
    echo ""
    echo "Attempting to establish connection with Flipper Zero..."
    echo "(Ensure USB cable is connected and Flipper is unlocked)"
    echo ""
    
    echo "IMPORTANT DIAGNOSTIC STEPS:"
    echo "1. Keep Flipper unlocked and CLI window active"
    echo "2. Watch for these log patterns:"
    echo "   - 'TX Start' or 'TX Begin' messages"
    echo "   - Frequency confirmation (432500000 Hz)"
    echo "   - Modulation parameters"
    echo "   - Byte count / packet length"
    echo "3. Listen on your radio for tone patterns"
    echo "   - Single continuous tone = malformed packet"
    echo "   - Chirp/modulation changes = packet being sent"
    echo "   - Interrupted signal = packet fragmentation"
    echo ""
    
    echo "Waiting 2 seconds before connecting..."
    sleep 2
    
    echo "Current time: $(date '+%H:%M:%S.%3N')"
    echo "═══════════════════════════════════════════════════════"
    echo "SubGHz Module Output Stream Begin"
    echo "═══════════════════════════════════════════════════════"
    echo ""
    
} | tee -a "$LOG_FILE"

# Run ufbt cli with timeout and timestamp each line
{
    timeout 120 ufbt cli 2>&1 || true
} | while IFS= read -r line; do
    TIMESTAMP_MS=$(date '+%H:%M:%S.%3N')
    echo "[$TIMESTAMP_MS] $line" | tee -a "$LOG_FILE"
done

{
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "SubGHz Module Output Stream End"
    echo "═══════════════════════════════════════════════════════"
    echo ""
    echo "End Time: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    
    echo "─────────────────────────────────────────────────────"
    echo "4. ANALYSIS CHECKLIST"
    echo "─────────────────────────────────────────────────────"
    echo ""
    echo "Check the logs above for the following:"
    echo ""
    echo "✓ TX Initialization:"
    echo "  - Look for: frequency set, oscillator init, PA settings"
    echo "  - Verify: 432500000 Hz in output"
    echo ""
    echo "✓ Packet Transmission:"
    echo "  - Count: How many bytes were transmitted?"
    echo "  - Timing: How long did transmission take?"
    echo "  - Completeness: Was packet transmitted fully?"
    echo ""
    echo "✓ Common Issues:"
    echo "  - [ ] Packet is fragmented into multiple TX calls"
    echo "  - [ ] TX is interrupted before completion"
    echo "  - [ ] Incorrect frequency selected"
    echo "  - [ ] Modulation parameters mismatch"
    echo "  - [ ] Preamble missing or truncated"
    echo "  - [ ] CRC calculation errors"
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "5. NEXT STEPS"
    echo "─────────────────────────────────────────────────────"
    echo ""
    echo "Review $LOG_FILE for:"
    echo "  1. SubGHz module initialization messages"
    echo "  2. TX start/end timestamps"
    echo "  3. Byte transmission sequence"
    echo "  4. Any error messages or warnings"
    echo ""
    echo "Compare radio reception with expected AX.25 packet pattern"
    echo ""
    
} | tee -a "$LOG_FILE"

echo ""
echo "✓ Diagnostic complete!"
echo "  Logs saved to: $LOG_FILE"
echo ""
echo "To review the logs:"
echo "  cat $LOG_FILE"
echo ""
