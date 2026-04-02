# APRS TX Interruption - Action Plan
## Step-by-Step Debugging and Resolution

**Issue:** APRS transmission breaks/cuts into packets during transmission  
**Suspected cause:** Incorrect bit duration timing or TX buffer overflow  
**Status:** Ready for diagnostic phase

---

## 🎯 Phase 1: Diagnostic Setup (15 minutes)

### Task 1.1: Verify Zed IDE Integration
- [x] Created `.zed/tasks.json` with 10 diagnostic tasks
- [x] Created diagnostic scripts (`diagnostic_tx.sh`, `analyze_tx_logs.sh`)
- [ ] **TODO: Test one task in Zed IDE**
  ```
  In Zed:
  1. Ctrl-Shift-P → "task: spawn"
  2. Select "Build APRS App"
  3. Verify compilation succeeds
  ```

### Task 1.2: Prepare CLI Monitoring
- [x] Created `UFBT_CLI_DETAILED_GUIDE.md` with all CLI commands
- [ ] **TODO: Establish Flipper CLI connection**
  ```bash
  ufbt cli
  > log_level APRS432 debug
  > log
  (keep this running during TX tests)
  ```

### Task 1.3: Prepare Radio Monitoring  
- [ ] **TODO: Set up radio on 432.500 MHz**
  - Frequency: 432.500 MHz USB/LSB (sideband mode)
  - BW: 2.5 or 5 kHz
  - Record audio if possible for analysis
  - Listen for tone pattern during TX

---

## 📊 Phase 2: Data Collection (20 minutes)

### Task 2.1: Capture TX Logs
In Zed IDE, run task: **"Log APRS TX Details (Extended Debug)"**

```
1. Launches 2-minute capture window
2. Wait for message: "Ready. Press TX..."
3. On Flipper: Press TX button
4. Watch radio for transmission
5. Wait for capture to complete (~120 seconds)
6. Output saved to: aprs_tx_detailed_YYYYMMDD_HHMMSS.log
```

**What the log should contain:**
- Timestamp of TX start
- Frequency confirmation (432500000)
- Modulation parameters
- Packet structure breakdown
- Hex dump of transmitted bytes
- TX completion message with byte count

### Task 2.2: Analyze First Capture

```bash
# View the complete log
cat aprs_tx_detailed_YYYYMMDD_HHMMSS.log

# Look for these patterns:
# ✅ GOOD: "TX Complete: 1234 bytes sent"
# ❌ BAD:  "TX interrupted" or "TX timeout"

# Extract just TX data:
grep "TX\|Preamble\|Address\|CRC" aprs_tx_detailed_*.log
```

### Task 2.3: Repeat with CLI Monitoring

In Flipper CLI (separate terminal):
```
log_clear
log_level APRS432 debug
log

# In Zed, run task: "Build & Launch APRS App"
# Then press TX on Flipper
# Watch both CLI output AND radio

# After 30 seconds, Ctrl+C in CLI
```

**Compare both outputs** - Zed task logs vs. CLI logs

---

## 🔍 Phase 3: Analysis (30 minutes)

### Task 3.1: Measure Packet Size

From captured log, extract all transmitted bytes:

```bash
# Count total bytes
grep "TX Data\|Packet bytes" log.log | wc -l

# Extract hex stream
grep "TX\[" log.log | awk '{print $2}' | tr '\n' ' '
```

**Expected values for typical position report:**
- Preamble: 32 bytes (0x7E repeated)
- Destination: 8 bytes
- Source: 8 bytes  
- Path (if 1 hop): 8 bytes
- Control+PID: 2 bytes
- Payload: 30-50 bytes (position, status, etc.)
- FCS: 2 bytes
- Postamble: 3 bytes
- **With bit-stuffing: ~10-15% increase**
- **TOTAL: 150-250 bytes** (not 1234!)

### Task 3.2: Check Bit Duration Value

In code: `aprs_hello_world_clean_app.c`

```c
#define APRS_GFSK_BIT_DURATION_US  104U
```

**Problem diagnosis:**
- 104 µs = 9600 baud (WRONG)
- Should be 833 µs = 1200 baud (CORRECT)
- **Ratio: 833/104 ≈ 8x** → Everything transmits 8x too fast!

This is likely cause #1.

### Task 3.3: Verify Transmission Structure

From hex dump in log, verify structure:

```
Position 0-31:   7E 7E 7E... (preamble flags)
Position 32-39:  ?? ?? ?? ?? ?? ?? ?? ??  (destination call + SSID)
Position 40-47:  ?? ?? ?? ?? ?? ?? ?? ??  (source call + SSID)
Position 48-55:  ?? ?? ?? ?? ?? ?? ?? ??  (repeater path if enabled)
Position 56-57:  03 F0  (control + PID)
Position 58+:    ?? ?? ?? ...  (payload data)
Position end-3:  ?? ??  (CRC checksum)
Position end-2:  7E  (postamble)
```

**Red flags:**
- If stops at position 48 → Path processing issue
- If stops at position 58 → Payload issue  
- If has gaps → Buffer fragmentation
- If no 0x7E at boundaries → Flag bit-stuffing issue

### Task 3.4: Check Timing

From CLI output or logs:

```
[HH:MM:SS.123] TX Start
[HH:MM:SS.456] TX Complete
Time difference = 333 ms

At 1200 baud: 333 ms / (1200/8) = 2.2 seconds data
But we only had 150-200 bytes = 1.3 seconds
Difference = 0.9 seconds overhead? ← Something is wrong!
```

---

## 🔧 Phase 4: Investigation (60 minutes)

### Task 4.1: Measure ACTUAL vs EXPECTED

Create comparison table:

| Parameter | Expected | Found in Log | Match? |
|-----------|----------|-------------|--------|
| Frequency | 432500000 Hz | ??? | ✓/✗ |
| Modulation | GFSK | ??? | ✓/✗ |
| Baud rate | 1200 | ??? | ✓/✗ |
| Preamble | 32 bytes | ??? | ✓/✗ |
| Total TX time | 1-2 sec | ??? | ✓/✗ |
| Bytes sent | 200-300 | ??? | ✓/✗ |

### Task 4.2: Examine Code for Timing Issue

Check `aprs_hello_world_clean_app.c`:

```c
// FIND THIS:
#define APRS_GFSK_BIT_DURATION_US  104U

// REASON IT'S WRONG:
// 1200 baud = 1 bit per (1,000,000 / 1200) µs
// 1,000,000 / 1200 = 833.33 µs

// Calculate what 104 µs means:
// 1,000,000 / 104 = 9615 baud (9.6x too fast!)

// This means:
// - Timing signals come 8x too fast
// - SubGHZ module might truncate
// - Receiver can't keep up
// - Packets appear to "cut off"
```

### Task 4.3: Check for Buffer Limits

In `furi_hal_subghz.h` (Flipper firmware):

```c
// Look for:
#define FURI_HAL_SUBGHZ_BUF_SIZE  ???

// If < 256, packet might be truncated
// If has timeout, transmission might be interrupted
```

### Task 4.4: Verify Encoding Logic

In `aprs_ax25_encoder.c`:

```c
// Check sequence:
// 1. Preamble loop (should add 32 bytes of 0x7E)
// 2. Destination field (8 bytes)
// 3. Source field (8 bytes)
// 4. Path fields if enabled (8 bytes each)
// 5. Control byte (0x03)
// 6. PID byte (0xF0)
// 7. Information field
// 8. Calculate CRC (with bit-stuffing!)
// 9. Postamble (3 × 0x7E)

// MISSING: Bit stuffing?
// AX.25 requires: after 5 consecutive '1' bits, insert a '0'
// This can increase payload by 10-15%
```

---

## ✅ Phase 5: Hypotheses to Test (Testing Order)

### Hypothesis 1: CRITICAL - Wrong Bit Duration ⭐⭐⭐

**Probability: 95%**

Fix: Change `APRS_GFSK_BIT_DURATION_US` from 104 to 833

**Test:**
1. Edit `aprs_hello_world_clean_app.c`
2. Change the constant
3. Build: Task → "Build & Launch APRS App"  
4. Test TX
5. Check if radio reception improves
6. Review logs for proper pacing

**Expected improvement:** Complete, smooth packet transmission

---

### Hypothesis 2: HIGH - TX Buffer Overflow

**Probability: 60%**

**Test:**
1. Reduce payload size
2. Disable one repeater path
3. Shorten status text
4. Re-test

**Expected improvement:** More complete transmission (if size was the issue)

---

### Hypothesis 3: MEDIUM - Missing Bit-Stuffing

**Probability: 40%**

**Test:**  
1. Check if CRC calculations include bit-stuffing
2. Verify encoded_length accounts for stuffed bits
3. Compare calculated vs. actual bytes sent

**Expected improvement:** CRC matches, receiver decodes properly

---

### Hypothesis 4: LOW - Preamble Too Short

**Probability: 25%**

Fix: Change `APRS_PREAMBLE_FLAGS` from 32 to 64

**Test:**
1. Edit constant
2. Re-test  
3. Should work better (but more overhead)

---

## 🔨 Phase 6: Implement Fix (20 minutes)

### FIX #1: Correct Bit Duration (MANDATORY)

**File:** `aprs_hello_world_clean_app.c`

**Change:**
```c
// LINE BEFORE:
#define APRS_GFSK_BIT_DURATION_US  104U

// LINE AFTER:
#define APRS_GFSK_BIT_DURATION_US  833U  // 1200 baud = 1/(1200) sec = 833.33 µs
```

### FIX #2: Increase Preamble (RECOMMENDED)

**File:** `aprs_hello_world_clean_app.c`

**Change:**
```c
// FROM:
#define APRS_PREAMBLE_FLAGS  32U

// TO:
#define APRS_PREAMBLE_FLAGS  64U  // More robust sync
```

### FIX #3: Add Debug Logging (OPTIONAL)

**File:** `aprs_hello_world_clean_app.c`

Add before TX:
```c
FURI_LOG_I(TAG, "TX: %u bytes, preamble=%u, duration_us=%u",
    encoded_length, 
    APRS_PREAMBLE_FLAGS * 8,  // 8 bits per flag byte
    APRS_GFSK_BIT_DURATION_US);
```

---

## 🧪 Phase 7: Testing & Validation (30 minutes)

### Test 1: Build Successfully

```
Zed Task: "Build APRS App"
Expected: ✓ No compilation errors
```

### Test 2: TX with Logging

```
Zed Task: "Log APRS TX Details (Extended Debug)"
Expected: 
  - Transmission completes
  - Full packet visible in logs  
  - Correct byte count
  - No timeout messages
```

### Test 3: Radio Reception

```
Listen on 432.500 MHz
Expected:
  - Smooth tone modulation
  - No tone variations/dropouts
  - Single continuous transmission (not choppy)
  - Can decode in direwolf or other TNC
```

### Test 4: Compare Logs

```bash
# If you kept old log:
diff aprs_tx_detailed_BEFORE.log aprs_tx_detailed_AFTER.log

# Should show:
# - Longer transmission time (now correct)
# - More bytes total (correct preamble)
# - Better structure
```

---

## 📋 Checklist Before & After Fix

### BEFORE Fix:
- [ ] Captured TX logs showing issue
- [ ] Identified byte count in log  
- [ ] Measured transmission duration
- [ ] Noted any error messages
- [ ] Documented what radio hears
- [ ] Saved baseline logs

### AFTER Fix #1 (bit duration):
- [ ] Rebuilt application
- [ ] Re-tested TX
- [ ] Compared transmission time
- [ ] Checked if radio reception improved
- [ ] Saved post-fix logs
- [ ] Documented improvement (or remaining issues)

### AFTER Fix #2 (preamble):
- [ ] Rebuilt application
- [ ] Re-tested TX  
- [ ] Compared reception quality
- [ ] Documented final result

---

## 🎯 Expected Progression

**Phase 1-2 BEFORE Fix:**
```
Radio output: "Chirp-chirp-chirp... [DROPOUT] ...chirp..."
Symptom: Interrupted transmission
Cause: 8x too fast baud rate
```

**Phase 1-2 AFTER Fix #1:**
```
Radio output: "Sssssscreee-screee [full modulation]... sss"
Symptom: Complete smooth transmission
Success: Issue RESOLVED
```

---

## 📞 If Things Don't Improve

If after applying fixes transmisison STILL breaks:

### Check these next:

1. **Verify code changes actually compiled:**
   ```bash
   grep APRS_GFSK_BIT_DURATION_US /path/to/build/app.c
   # Should show 833, not 104
   ```

2. **Check SubGHZ module capacity:**
   ```bash
   ufbt cli
   > subghz_test_static
   # Look for buffer size, frequency limits, etc.
   ```

3. **Try even longer preamble:**
   ```c
   #define APRS_PREAMBLE_FLAGS  128U  // Double again
   ```

4. **Reduce payload:**
   - Shorter status text
   - Remove repeater paths temporarily
   - Test with minimal position-only packet

5. **Check CRC/bit-stuffing:**
   - Study `aprs_ax25_encoder.c` carefully
   - Verify all bits are being encoded
   - Test CRC calculation offline

---

## 📝 Documentation Files Created

### For Users:
1. **`.zed/tasks.json`** - 10 ready-to-use Zed IDE tasks
2. **`md/ZED_IDE_TASKS_GUIDE.md`** - Guide for using Zed tasks
3. **`md/UFBT_CLI_DETAILED_GUIDE.md`** - Detailed CLI commands
4. **`md/PROBLEM_ANALYSIS_TX_INTERRUPTION.md`** - Full technical analysis

### For Diagnostics:
5. **`diagnostic_tx.sh`** - Automated diagnostic script
6. **`analyze_tx_logs.sh`** - Log analysis utility
7. **Generated logs** - `diagnostic_logs/` folder, `tx_*.log` files

---

## 🎬 Starting Right Now

### Do this first (5 minutes):

1. Open Zed IDE in this project folder
2. Press Ctrl-Shift-P
3. Type "task" → select "task: spawn"
4. Select: "Build APRS App"
5. Verify it compiles without errors

### Then (10 minutes):

6. From task menu again, select: "Open Flipper CLI (Debug Mode)"
7. Type commands (in the Flipper CLI that opens):
   ```
   log_level APRS432 debug
   log
   ```
8. Leave this running

### Then (15 minutes):

9. Back in Zed, select task: "Build & Launch APRS App"
10. Wait for it to complete
11. On your Flipper device, press the TX button
12. Watch both the CLI output and listen on radio

### Finally (5 minutes):

13. Stop CLI (Ctrl+C)
14. Review the output for the issues mentioned in `PROBLEM_ANALYSIS_TX_INTERRUPTION.md`

---

## 🏁 Success Criteria

✅ **Issue RESOLVED when:**

1. **TX completes without timeout:** Log shows "TX Complete: XXX bytes"
2. **Radio receives clean signal:** No dropout/interruption on 432.500
3. **Proper transmission time:** ~1-2 seconds for typical packet (not 0.1 seconds)
4. **Direwolf decodes properly:** If you have direwolf running, it should decode the APRS packet
5. **No error messages:** CLI shows no "TX failed", "timeout", or "error"

---

**Next step:** Start with Phase 1, Task 1.1 - Open Zed and run "Build APRS App" task

