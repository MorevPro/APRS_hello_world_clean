# APRS Transmission Interruption Diagnosis
## Flipper Zero SubGHz Module Analysis

**Date:** 2026-04-03  
**Issue:** Transmission appears to break or cut into packets; radio receives garbled tones/incomplete signal  
**Symptom:** Different pitch tones heard instead of smooth APRS modulation  

---

## Problem Summary

The APRS application transmits data to the Flipper Zero's SubGHZ module, which should modulate it as a continuous GFSK signal on 432.5 MHz. However, you're observing:

1. **Incomplete transmission** - Signal stops mid-transmission
2. **Packet fragmentation** - Data appears split across multiple TX cycles  
3. **Tone variations** - Radio hears different pitches (indicates modulation/packet boundary artifacts)
4. **Signal dropout** - Transmission is interrupted rather than continuous

---

## Technical Diagnosis

### Current APRS Configuration

```c
// From aprs_hello_world_clean_app.c
#define APRS_FREQUENCY_HZ          432500000UL    // 432.5 MHz
#define APRS_GFSK_BIT_DURATION_US  104U            // ~1200 baud (1,000,000/104 ≈ 9.6k bits/s, but AX.25 is 1200)
#define APRS_PREAMBLE_FLAGS        32U             // 256 bits preamble
#define APRS_POSTAMBLE_FLAGS       3U              // 24 bits postamble
```

### Expected AX.25 Packet Structure

```
┌─────────────────────────────────────────────┐
│ PREAMBLE (32 × 0x7E = 256 bits)            │  = 213 ms @ 1200 baud
├─────────────────────────────────────────────┤
│ FLAG SEQUENCE (0x7E = 0111 1110)            │  = 6.67 ms
├─────────────────────────────────────────────┤
│ DESTINATION ADDRESS (7 bytes)               │  = 46.67 ms
│ + SSID Field (1 byte)                       │
├─────────────────────────────────────────────┤
│ SOURCE ADDRESS (7 bytes)                    │  = 46.67 ms
│ + SSID Field (1 byte)                       │
├─────────────────────────────────────────────┤
│ PATH FIELDS (optional, N × 8 bytes)         │  = 53.33 ms per hop
├─────────────────────────────────────────────┤
│ CONTROL (0x03) + PID (0xF0) = 2 bytes       │  = 13.33 ms
├─────────────────────────────────────────────┤
│ INFORMATION FIELD (payload, variable)       │  = depends on content
│  - Position/Status: ~30-50 bytes            │  = 200-333 ms
├─────────────────────────────────────────────┤
│ FCS (CRC, 2 bytes)                          │  = 13.33 ms
├─────────────────────────────────────────────┤
│ POSTAMBLE (3 × 0x7E = 24 bits)              │  = 20 ms
└─────────────────────────────────────────────┘
```

**Total expected transmission time: 500-800 ms for typical position report**

---

## Likely Root Causes

### 1. **Preamble Truncation**
**Symptom:** Receiver misses initial flag synchronization  
**Evidence to check:**
- Is preamble actually being transmitted?
- Check if `APRS_PREAMBLE_FLAGS = 32` is enough
- APRS spec recommends 100+ flag bytes for reliable reception

**Code section to investigate:**
```c
aprs_ax25_encoder.c
// Verify preamble transmission before frame data
```

### 2. **Incomplete Packet Transmission**
**Symptom:** TX stops before FCS and postamble  
**Evidence to check:**
- Does transmission stop after certain byte count?
- Is there timeout/error causing TX abort?
- Check SubGHZ HAL buffer limits

**Critical check:**
```c
furi_hal_subghz_transmit() 
// Does it send entire buffer or has length limits?
```

### 3. **TX Buffer Overflow/Fragmentation**
**Symptom:** Packet split into multiple TX calls  
**Evidence to check:**
- How large is SubGHZ TX buffer?
- Is packet data being sent in chunks?
- Are there gaps between chunks?

```c
// From furi_hal_subghz.h
#define FURI_HAL_SUBGHZ_BUF_SIZE  // Unknown - NEED TO CHECK
```

### 4. **Bit Rate/Timing Mismatch**
**Symptom:** Receiver can't lock onto modulation  
**Problem:** `APRS_GFSK_BIT_DURATION_US = 104U` may be incorrect

**Calculation:**
- 1200 baud = 1 bit per 833.33 µs
- Current setting (104 µs) = ~9600 baud (incorrect!)

**This is likely the PRIMARY issue!**

### 5. **Modulation Parameter Issue**
**Symptoms that indicate this:**
- Different tones = incorrect deviation
- Scratchy/noisy = incorrect bandwidth or filter
- Can't decode = incorrect modulation type

---

## Diagnostic Procedure

### Step 1: Collect Raw TX Data

Use the provided task in Zed IDE:

```bash
# From .zed/tasks.json - run "Log APRS TX Details (Extended Debug)"
ufbt cli 2>&1 | tee tx_capture_$(date +%Y%m%d_%H%M%S).log
```

**Expected output includes:**
```
[timestamp] TX Start: frequency=432500000
[timestamp] Modulation params: ...
[timestamp] TX Data: [hex dump of bytes]
[timestamp] TX Complete
```

### Step 2: Check What Gets Transmitted

Add debug logging to capture actual SubGHZ transmission:

```c
// In aprs_hello_world_clean_app.c, add to TX function:

FURI_LOG_I(TAG, "Total TX bytes: %u", encoded_length);
for(uint16_t i = 0; i < encoded_length; i += 32) {
    char hex_str[97] = {0};
    uint32_t chunk_len = (encoded_length - i) > 32 ? 32 : (encoded_length - i);
    for(uint32_t j = 0; j < chunk_len; j++) {
        snprintf(&hex_str[j*2], 3, "%02X", encoded_data[i+j]);
    }
    FURI_LOG_I(TAG, "TX[%u-%u]: %s", i, i+chunk_len-1, hex_str);
}
```

### Step 3: Monitor Transmission Length

```c
// Track actual bytes sent
size_t sent_bytes = furi_hal_subghz_transmit(encoded_data, encoded_length);
FURI_LOG_I(TAG, "Requested: %u bytes, Sent: %u bytes", encoded_length, sent_bytes);
```

### Step 4: Verify AX.25 Packet Structure

Expected pattern for test packet:
```
Preamble:  7E 7E 7E 7E 7E 7E 7E 7E ... (32× = 256 bits)
Frame:     7E [DEST] [SRC] 03 F0 [DATA] [CRC] 7E
          └─ Flags mark packet boundaries
```

---

## Common Code Issues to Check

### Issue 1: Incorrect Bit Duration

**In `aprs_hello_world_clean_app.c`:**

```c
#define APRS_GFSK_BIT_DURATION_US  104U  // ❌ LIKELY WRONG!
```

**Should be** (for 1200 baud):
```c
#define APRS_GFSK_BIT_DURATION_US  833U  // Correct for 1200 baud
// or 1200 µs for 833.33 bauds with rounding
```

### Issue 2: Missing or Incomplete Encoding Chain

Check in `aprs_ax25_encoder.c`:
```c
// Verify this sequence:
// 1. Preamble flags (32×)
// 2. Destination + SSID (8 bytes)
// 3. Source + SSID (8 bytes)
// 4. Path (if enabled)
// 5. Control (0x03)
// 6. PID (0xF0)
// 7. Information field
// 8. CRC calculation (with bit stuffing!)
// 9. Postamble flags (3×)
```

**CRITICAL:** AX.25 uses **bit stuffing** - after 5 consecutive 1-bits, a 0-bit is inserted. This can increase packet size by 10-15%!

### Issue 3: SubGHZ HAL Buffer Limitations

```c
// Check furi_hal_subghz.h for:
- Maximum packet size
- Whether TX has timeout
- If buffer is flushed between TX calls
```

---

## Recommended Fixes

### Fix 1: Correct Bit Duration (PRIORITY 1)

```c
// Change in aprs_hello_world_clean_app.c or config
#define APRS_GFSK_BIT_DURATION_US  833U  // 1200 baud
// OR if using Flipper's preset configs:
#define APRS_GFSK_BIT_DURATION_US  1200U  // Alternative timing
```

### Fix 2: Add Transmission Verification

```c
// After TX, verify bytes were sent
static void aprs_tx_packet(APRSHelloWorldCleanApp* app) {
    // ... encoding logic ...
    
    size_t total_bytes = encoded_length;
    FURI_LOG_I(TAG, "TX Start: %u bytes @ 432.5MHz", total_bytes);
    
    size_t sent = furi_hal_subghz_transmit(encoded_data, encoded_length);
    
    if(sent != total_bytes) {
        FURI_LOG_W(TAG, "TX Incomplete: sent %u of %u bytes", sent, total_bytes);
        // Log which bytes were missed
    } else {
        FURI_LOG_I(TAG, "TX Complete: all %u bytes transmitted", sent);
    }
}
```

### Fix 3: Increase Preamble

```c
#define APRS_PREAMBLE_FLAGS  64U   // Increase from 32 to 64 for reliable sync
// This adds only ~400ms extra, worth it for reliability
```

### Fix 4: Add Debug Output During TX

```c
// In TX function, add before furi_hal_subghz_transmit():
FURI_LOG_I(TAG, "Packet structure:");
FURI_LOG_I(TAG, "├─ Preamble: %u bytes", preamble_len);
FURI_LOG_I(TAG, "├─ Destination: 8 bytes");
FURI_LOG_I(TAG, "├─ Source: 8 bytes");  
FURI_LOG_I(TAG, "├─ Path: %u bytes", path_len);
FURI_LOG_I(TAG, "├─ Control+PID: 2 bytes");
FURI_LOG_I(TAG, "├─ Payload: %u bytes", payload_len);
FURI_LOG_I(TAG, "├─ FCS: 2 bytes");
FURI_LOG_I(TAG, "└─ Postamble: 3 bytes");
FURI_LOG_I(TAG, "Total: %u bytes (including bit stuffing)", total_encoded);
```

---

## Testing Procedure

1. **Build with debug enabled:**
   ```bash
   ufbt      # Compile
   ```

2. **Flash to Flipper:**
   ```bash
   ufbt FORCE=1 flash_usb
   ```

3. **Start CLI monitoring:**
   ```bash
   ufbt cli
   ```

4. **In another terminal, transmit:**
   - Click TX on Flipper app
   - Watch CLI output for packet structure logs

5. **Check radio reception:**
   - Listen on 432.500 MHz USB (LSB sideband)
   - Record audio if possible
   - Check if received pattern matches expected AX.25

6. **Compare logs:**
   ```bash
   # From log, verify:
   # - Number of bytes transmitted
   # - Presence of preamble
   # - Complete packet structure
   ```

---

## Expected vs. Actual

### Expected (Correct) Log Output
```
[14:23:45.123] TX Start: 1234 bytes @ 432.5MHz
[14:23:45.124] Preamble: 32 bytes
[14:23:45.125] Destination: 8 bytes  
[14:23:45.126] Source: 8 bytes
[14:23:45.128] Control+PID: 2 bytes
[14:23:45.130] Payload: 45 bytes
[14:23:45.132] FCS: 2 bytes
[14:23:45.133] Postamble: 3 bytes
[14:23:46.250] TX Complete: 1234 bytes transmitted successfully
```

### Actual (If Broken - Example)
```
[14:24:10.500] TX Start: 1234 bytes @ 432.5MHz
[14:24:10.501] TX transmitted 512 bytes
[14:24:10.502] ERROR: Transmission stopped early
```

---

## Files to Modify

### Primary suspects in codebase:
1. `aprs_hello_world_clean_app.c` - TX logic and timing constants
2. `aprs_ax25_encoder.c` - Packet encoding, bit stuffing, CRC
3. `aprs_ax25_encoder.h` - Timing/size defines
4. `aprs_config.h` - Configuration constants

### Compile and test:
```bash
# From .zed/tasks.json, use:
# "Build & Launch APRS App" - builds and launches
# "Log APRS TX Details (Extended Debug)" - captures TX
```

---

## Hypothesis Ranking

| Priority | Issue | Probability | Impact |
|----------|-------|------------|--------|
| 1 | Incorrect bit duration (104µs vs 833µs) | **95%** | **CRITICAL** - Causes complete failure |
| 2 | Incomplete TX due to buffer limit | **60%** | **HIGH** - Packet truncation |
| 3 | Missing bit stuffing in encoder | **40%** | **HIGH** - CRC failure |
| 4 | Modulation parameter mismatch | **30%** | **MEDIUM** - Weak signal |
| 5 | Preamble too short | **25%** | **LOW** - Intermittent decode |

---

## Next Actions

1. ✅ **Create Zed tasks for diagnostics** (DONE - `.zed/tasks.json`)
2. ⏳ **Run TX data capture using diagnostic tasks**
3. ⏳ **Analyze captured logs for packet structure**
4. ⏳ **Fix bit duration constant** (LIKELY FIX #1)
5. ⏳ **Increase preamble length** (EASY IMPROVEMENT)
6. ⏳ **Add transmit verification logging** (CONFIRMS FIX)
7. ⏳ **Re-test with corrected parameters**
8. ⏳ **Verify radio reception matches AX.25 spec**

---

## Resources

- **AX.25 Standard:** Bell 202 1200 baud AFSK
- **Flipper SubGHz HAL:** Check `furi_hal_subghz.h` for buffer limits
- **APRS Spec:** Continuous packet transmission at 1200 baud
- **Expected modulation:** 1200 Hz mark, 2200 Hz space (classic APRS)

---

## Debug Logging Commands

Ready to run:
```bash
# Terminal 1: Start CLI monitor
ufbt cli

# Terminal 2: From project directory
# Run one of these Zed tasks:
# - "Save TX Logs to File (1 minute capture)"
# - "Log APRS TX Details (Extended Debug)"  
# - "Analyze Last TX Log"
```

---

**Last Updated:** 2026-04-03  
**Status:** PENDING TX DATA CAPTURE AND ANALYSIS  
**Next Review:** After diagnostic logs are collected

