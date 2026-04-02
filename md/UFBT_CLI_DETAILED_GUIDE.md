# ufbt CLI Commands for APRS TX Debugging
## Maximum Detail Log Capture Methods

---

## 📌 Overview

The Flipper Zero `ufbt cli` command opens a serial connection to the device's CLI interface. You can:
- View real-time logs from the application
- Execute Flipper commands directly
- Monitor SubGHZ module behavior
- Capture packet transmission details

---

## 🔌 Basic Connection

### From Zed IDE Task:
```
Select: "Open Flipper CLI (Debug Mode)" from task menu
```

### From Terminal:
```bash
ufbt cli
```

**Result:** Interactive CLI prompt
```
loading_tasks_queue
>
```

---

## 📝 Key Commands for APRS Debugging

### 1. View All Logs
```
> log
```
Shows all recent logs with timestamps:
```
[timestamp] [MODULE] MESSAGE
[14:23:45.012] [APRS432] TX Start: 1234 bytes
[14:23:45.013] [APRS432] Preamble: 32 bytes  
[14:23:46.123] [APRS432] TX Complete
```

### 2. Filter Logs by Module
```
> log_level APRS432 debug
```
Enable detailed logging for APRS432 tag:
```
[14:23:45.012] [APRS432_D] Packet structure:
[14:23:45.012] [APRS432_D] ├─ Destination: 00 E0 00 00... 
[14:23:45.012] [APRS432_D] ├─ Source: 18 E0 00 00...
```

### 3. Monitor Specific Tag
```
> log_cli APRS432
```
Show only APRS432 module logs (high performance)

### 4. SubGHZ Module Status
```
> subghz_test_static
```
Test SubGHZ module configuration:
```
Working frequency: 432500000 Hz
Modulation: GFSK
...
```

### 5. Enable Maximum Debug for SubGHZ
```
> log_level subghz_devices debug
> log_level subghz_worker debug  
> log_level subghz_tx debug
```

---

## 🎯 Best Command Sequences for TX Diagnostics

### Sequence 1: Complete TX Capture with Detailed Output

```bash
# In Flipper CLI:
log_level debug                      # Enable all debug logs
log_level APRS432 trace              # Maximum detail for APRS
subghz_test_static                   # Verify SubGhZ config
log                                  # Now start monitoring
```

Then press TX on Flipper. Output should show:
```
[14:23:45.123] [APRS432] TX Start: frequency=432500000
[14:23:45.124] [APRS432] Payload bytes: [7E 7E 7E... (hex)
[14:23:45.200] [APRS432] TX modulation: GFSK
[14:23:46.400] [APRS432] TX Complete: 1234 bytes sent
```

### Sequence 2: Focused SubGHZ Monitoring

```bash
# Enable only SubGHZ module debug
log_clear
log_level subghz_devices debug
log_level subghz_worker debug
log
```

This captures:
- Device initialization
- TX buffer operations
- Modulation parameters
- Timing information

### Sequence 3: Packet Structure Analysis

```bash
# Custom app-level logging
log_level APRS432 trace
log
# Press TX, then:
log_cli APRS432
```

Gives raw packet bytes with metadata:
```
[14:23:45.123] Packet bytes: 7E 7E 7E 7E...
[14:23:45.124] Address field: NX5PMR-0 
[14:23:45.125] Destination: RELAY-0
[14:23:45.126] Control: 03 F0
[14:23:45.127] Payload: Position report
[14:23:45.128] CRC: 12 34
```

---

## 💾 Saving Output to File

### Method 1: From Terminal with tee

```bash
# In your shell (not Flipper CLI):
ufbt cli 2>&1 | tee tx_debug_$(date +%Y%m%d_%H%M%S).log
```

Then in the Flipper CLI prompt that appears:
```
> log_level APRS432 debug
> log
# Press TX on Flipper
# Wait 30 seconds
# Ctrl+C to stop
```

File saved with all output.

### Method 2: From Zed IDE Task

The pre-configured task already does this:
```
"Log APRS TX Details (Extended Debug)"
```

Generates:
```
aprs_tx_detailed_20260403_142345.log
```

### Method 3: Using script

```bash
# Save each line with millisecond timestamp
ufbt cli 2>&1 | while read line; do
    echo "[$(date +%T.%3N)] $line"
done | tee diagnostic_$(date +%s).log
```

---

## 🔍 What to Look For in Output

### ✅ Healthy TX Signature:
```
[14:23:45.100] [APRS432] TX Start: frequency=432500000, modulation=GFSK
[14:23:45.101] [APRS432] TX Data (bytes 0-31): 7E 7E 7E 7E 7E 7E 7E 7E...
[14:23:45.150] [APRS432] TX Data (bytes 32-63): 7E 82 A0 C4 40 E0 60 A0...
[14:23:45.200] [APRS432] TX Data (bytes 64-95): C8 60 62 8E 68 88 84 84...
[14:23:46.400] [APRS432] TX Complete: 1234 bytes, duration=1300ms
```

### ❌ Problem Signatures:

**Transmission interrupted:**
```
[14:23:45.100] TX Start: 1234 bytes
[14:23:45.200] TX Data (bytes 0-31): 7E 7E...
[14:23:45.300] ERROR: TX timeout or buffer exceeded
[14:23:45.301] TX aborted: 300 bytes sent of 1234
```

**Incomplete packet:**
```
[14:23:45.100] TX Start: 1234 bytes
[14:23:45.100] Preamble: OK (32 bytes)
[14:23:45.101] Address fields: OK (16 bytes)
[14:23:45.102] Payload start...
[14:23:45.103] ERROR: Packet encoding failed
```

**Modulation mismatch:**
```
[14:23:45.100] TX frequency: 432500000 Hz ← OK
[14:23:45.101] Frequency deviation: 5000 Hz ← ❌ SHOULD BE 2500
[14:23:45.102] TX bitrate: 9600 baud ← ❌ SHOULD BE 1200
```

**Timing problem:**
```
[14:23:45.100] TX Start: 1234 bytes
[14:23:45.101] Bit duration: 104 µs ← ❌ SHOULD BE 833 µs
[14:23:45.102] Expected duration: 10.3 ms ← ❌ SHOULD BE 10.3 seconds
```

---

## 🎬 Live Debugging Session Example

### Setup:
Have 3 terminals/panes:
1. **Zed IDE** - Full task runner
2. **Terminal 1** - Flipper CLI
3. **Terminal 2** - Radio monitoring (or spectral analyser)

### Procedure:

**Terminal 1: Start CLI**
```bash
$ ufbt cli 2>&1 | tee session_$(date +%s).log
loading_tasks_queue
> log_level APRS432 debug
> log
```

**Zed IDE:** Meanwhile, edit code
```
aprs_hello_world_clean_app.c
Add debug output: FURI_LOG_I(TAG, "About to TX...");
Save file
```

**Run:** Task → "Build & Launch APRS App"

**Terminal 2:** Monitor radio
```bash
$ rtl_fm -f 432.5M -s 48k -A fast -l 100 | aplay
```

**Back to Terminal 1:** Watch logs appear as you press TX

**Copy output:**
```bash
# After testing, save complete session:
$ cp session_1234567890.log ../tx_diagnostics/
```

---

## 🔧 Advanced: Custom Debug Code

To get even MORE detail, add to `aprs_hello_world_clean_app.c`:

```c
// Before TX:
FURI_LOG_I(TAG, "=== TX PACKET DEBUG ===");
FURI_LOG_I(TAG, "Total size: %u bytes", encoded_length);

// Dump in 32-byte chunks:
for(uint16_t i = 0; i < encoded_length; i += 32) {
    char hex_line[97] = {0};
    uint32_t chunk = (encoded_length - i > 32) ? 32 : (encoded_length - i);
    
    for(uint32_t j = 0; j < chunk; j++) {
        char hex_byte[3];
        snprintf(hex_byte, sizeof(hex_byte), "%02X", encoded_data[i+j]);
        strcat(hex_line, hex_byte);
        if((j + 1) % 4 == 0) strcat(hex_line, " ");
    }
    
    FURI_LOG_I(TAG, "TX[%4u-%4u]: %s", i, i+chunk-1, hex_line);
}

// After TX:
FURI_LOG_I(TAG, "=== TX COMPLETE ===");
```

Then in CLI:
```
> log_level APRS432 info
> log
# Press TX
# See perfect hex dump of transmitted bytes
```

---

## 📊 Log File Analysis

After capturing logs to file:

### Count TX events:
```bash
grep -c "TX Start\|TX Complete" tx_debug_*.log
```

### Extract only SubGHZ output:
```bash
grep "subghz\|TX\|modulation" tx_debug_*.log
```

### Find errors:
```bash
grep -i "error\|timeout\|fail\|abort" tx_debug_*.log
```

### Get timing information:
```bash
grep "\[.*\]" tx_debug_*.log | head -20
```

### Compare two sessions:
```bash
diff <(grep "TX Data" session1.log) <(grep "TX Data" session2.log)
```

---

## ⚡ Quick Reference Table

| Goal | Command(s) |
|------|-----------|
| **View all logs** | `log` |
| **APRS debug only** | `log_level APRS432 debug` → `log` |
| **SubGHZ details** | `log_level subghz_* debug` → `log` |
| **Hex packet dump** | Add custom logging to code |
| **Real-time monitoring** | `log` + press TX |
| **Save to file** | `ufbt cli 2>&1 \| tee file.log` |
| **Module list** | `log_list_tags` |
| **Clear old logs** | `log_clear` |
| **High performance** | `log_cli APRS432` |

---

## 🚨 Troubleshooting CLI Issues

### CLI won't connect:
```
❌ "Port not found"
   → Check USB cable
   → Unlock Flipper screen
   → Try: ls /dev/ttyACM* (Linux)

❌ "Timeout"
   → Restart Flipper
   → Try different USB port
   → Update ufbt: ufbt update
```

### Output incomplete:
```
❌ "Last messages cut off"
   → Use tee to save to file simultaneously
   → Increase terminal buffer size
   → Save to file for complete history
```

### No debug output:
```
❌ "log_level command not found"
   → May need firmware rebuild
   → Check ufbt cli 2>&1 | head -20 for version info
   → Try: log_list_tags to see available modules
```

---

## 📋 Session Template

```bash
# Copy-paste this into ufbt cli for full diagnostic:

log_clear
log_list_tags
log_level debug
log_level APRS432 trace
subghz_test_static
echo "=== Ready. Press TX on Flipper now ==="
log

# After TX, capture log or Ctrl+C
```

---

## 🎯 Final Checklist for TX Diagnostics

- [ ] Flipper connected and unlocked
- [ ] Ran: `log_level APRS432 debug`
- [ ] Ran: `log` command
- [ ] Pressed TX button
- [ ] Observed complete transmission
- [ ] Saved output to file
- [ ] Identified all hex bytes transmitted
- [ ] Checked byte count against expected
- [ ] Verified packet structure (preamble, addresses, data, FCS)
- [ ] Noted any timeout/error messages
- [ ] READY: Compare against PROBLEM_ANALYSIS_TX_INTERRUPTION.md

---

**Next:** Use this guide with Zed IDE tasks for maximum insight into TX behavior

