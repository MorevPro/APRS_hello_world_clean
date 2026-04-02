# Zed IDE for APRS TX Debugging
## Quick Start Guide for SubGHz Transmission Diagnostics

### What is Zed?

Zed is a high-performance code editor with built-in task support. Tasks let you run commands directly from the editor without switching to terminal windows.

---

## Available Tasks (in `.zed/tasks.json`)

### 📝 Build & Basic Operations

#### 1. **Build APRS App**
- **What:** Compiles the APRS application
- **When:** After code changes
- **Command:** `ufbt`
- **Time:** ~30-60 seconds

#### 2. **Build & Launch APRS App**  
- **What:** Compiles and immediately launches on Flipper
- **When:** Testing UI changes or app logic
- **Command:** `ufbt launch`
- **Time:** ~45-90 seconds

#### 3. **Clean Build**
- **What:** Removes build artifacts and recompiles from scratch
- **When:** Troubleshooting compilation issues
- **Command:** `ufbt -c`
- **Time:** ~60-120 seconds

---

### 🔍 TX Monitoring & Diagnostics

#### 4. **Open Flipper CLI (Debug Mode)**
- **What:** Opens interactive CLI session with Flipper
- **When:** Need to run commands on Flipper directly
- **Key point:** Runs in new terminal tab
- **Usage:** Type commands like `log` to see real-time logs

#### 5. **Monitor SubGHz TX (via CLI)**
- **What:** Captures TX events with file logging
- **Output:** `tx_capture_YYYYMMDD_HHMMSS.log`
- **Best for:** Real-time monitoring + file backup
- **Run time:** Manual (until you press Ctrl+C)

#### 6. **Save TX Logs to File (1 minute capture)**
- **What:** Auto-captures 60 seconds of TX data to file
- **Output:** `tx_logs_YYYYMMDD_HHMMSS.log`
- **Best for:** Quick testing window
- **Automatic:** Stops after 60 seconds

#### 7. **Log APRS TX Details (Extended Debug)** ⭐
- **What:** 2-minute capture with structured format
- **Output:** `aprs_tx_detailed_YYYYMMDD_HHMMSS.log`
- **Best for:** Complete packet structure analysis
- **Includes:** Metadata, start/end times, full packet data

#### 8. **Flash & Test TX (with logging)**
- **What:** Flash app → wait 2 seconds → start monitoring
- **Output:** Real-time TX logs
- **Best for:** Full cycle: compile, deploy, test
- **Time:** ~90 seconds compile + monitoring

---

## 🚀 Typical Debugging Workflow

### Scenario 1: "TX seems to cut off"

```
1. Open .zed/tasks.json in Zed
   cmd-shift-p → "task: spawn"
   
2. Select: "Build & Launch APRS App"
   ← Waits for app to be ready on device
   
3. Switch to terminal (stays open)
   
4. Back to Zed, select: "Log APRS TX Details (Extended Debug)"
   ← Starts 2-minute capture window
   
5. On Flipper: Press TX button
   
6. Wait for capture to complete (or press Ctrl+C)
   
7. Review log file:
   cat aprs_tx_detailed_YYYYMMDD_HHMMSS.log
```

### Scenario 2: "I want to check real-time logs"

```
1. Task: "Open Flipper CLI (Debug Mode)"
   ← Opens true interactive CLI
   
2. Type: log
   ← See all Flipper logs in real-time
   
3. On Flipper: Press TX button
   
4. Watch log output for:
   - TX start messages
   - Frequency confirmation  
   - Byte counts
   - Completion messages
   
5. Press Ctrl+C to exit CLI
```

### Scenario 3: "Quick test cycle"

```
For each code change:

1. Modify code (e.g., aprs_hello_world_clean_app.c)
   
2. Task: "Build & Launch APRS App"
   
3. Task: "Save TX Logs to File (1 minute capture)"
   
4. Test on radio
   
5. Review: cat tx_logs_*.log | tail -50
```

---

## 📊 How to Interpret TX Logs

### What to look for:

```
[HH:MM:SS.mmm] TX Start: frequency=432500000
                 ↓
              Shows SubGHz module is initialized

[HH:MM:SS.mmm] Modulation: GFSK, params...
                 ↓
              Confirms correct modulation

[HH:MM:SS.mmm] TX Data: 7E 7E 7E... (hex bytes)
                 ↓
              Shows preamble (7E = flag byte)

[HH:MM:SS.mmm] APRS Packet: DEST[8] SRC[8] DATA[40+]
                 ↓
              Shows packet structure decoded

[HH:MM:SS.mmm] TX Complete: 1234 bytes
                 ↓
              Confirms full transmission
```

### Red flags to watch for:

```
❌ "TX interrupted"        → Packet cut off early
❌ "Buffer overflow"       → packet too large
❌ "Timeout"               → TX took too long
❌ "CRC error"             → Calculate error in packet
❌ "No preamble detected"  → Missing initial flags
```

---

## 🔧 Keyboard Shortcuts in Zed

```
ctrl-shift-p     Open command palette
                 → Type "task" → "task: spawn"

ctrl-shift-r     Rerun last task (if configured)

ctrl-c           Stop/interrupt task (in terminal)
```

---

## 📁 Generated Files

After running diagnostics, you'll have:

```
project_root/
├── diagnostic_logs/
│   ├── aprs_tx_diagnostic_YYYYMMDD_HHMMSS.log
│   ├── tx_packets_YYYYMMDD_HHMMSS.txt
│   └── ... (more captures)
├── tx_capture_YYYYMMDD_HHMMSS.log
├── tx_logs_YYYYMMDD_HHMMSS.log
└── aprs_tx_detailed_YYYYMMDD_HHMMSS.log
```

**Keep these files!** They're crucial for:
- Comparing before/after fixes
- Identifying patterns in failures
- Performance analysis

---

## 📝 Troubleshooting Tasks

### Task won't run?

```
❌ "Command not found: ufbt"
   → Check: ufbt --version in terminal
   → May need to restart Zed after ufbt install

❌ "Flipper not detected"  
   → Check USB cable
   → Make sure Flipper is unlocked
   → Try: ufbt cli (manual terminal test)

❌ "Permission denied"
   → On Linux/Mac: chmod +x diagnostic_tx.sh
   → On Windows: Run Zed as admin (usually not needed)
```

### Task output cut off?

```
Task output limited to editor view.
Full output saved to .log files automatically.

To see complete output:
→ Open log file in Zed
→ Or: cat filename.log | less (terminal)
```

---

## 🎯 Recommended Debug Sequence

**STEP 1: Verify Setup**
```bash
Task: "Build APRS App"
     ↓ Should complete without errors
```

**STEP 2: Check Device Connection**
```bash  
Task: "Open Flipper CLI (Debug Mode)"
Type:  help
     ↓ Should show Flipper command list
```

**STEP 3: Capture TX Data**
```bash
Task: "Log APRS TX Details (Extended Debug)"
     ↓ 2-minute capture window
     ↓ Press TX on Flipper during capture
     ↓ Review output file
```

**STEP 4: Analyze Issues**
```bash
Review logs in: PROBLEM_ANALYSIS_TX_INTERRUPTION.md
Compare output against expected packet structure
```

**STEP 5: Implement Fix**  
```bash
Edit: aprs_hello_world_clean_app.c
Change: #define APRS_GFSK_BIT_DURATION_US 104U
To:     #define APRS_GFSK_BIT_DURATION_US 833U
```

**STEP 6: Re-test**
```bash
Task: "Build & Launch APRS App"  
Task: "Log APRS TX Details (Extended Debug)"
Verify: Improved packet structure in output
```

---

## 📋 Checklist for Complete Diagnostics

- [ ] Ran "Log APRS TX Details" task
- [ ] Pressed TX button on Flipper during capture
- [ ] Saved output log file
- [ ] Reviewed `PROBLEM_ANALYSIS_TX_INTERRUPTION.md`
- [ ] Checked for "incomplete transmission" errors
- [ ] Verified preamble flags (0x7E) in hex output
- [ ] Counted total bytes transmitted
- [ ] Compared with expected AX.25 packet size
- [ ] Checked for bit-stuffing artifacts
- [ ] Noted any timeout or buffer errors
- [ ] READY: Update code based on findings

---

## 🔗 Related Files

- **Tasks definition:** `.zed/tasks.json`
- **Problem analysis:** `md/PROBLEM_ANALYSIS_TX_INTERRUPTION.md`  
- **Diagnostic script:** `diagnostic_tx.sh`
- **Log analyzer:** `analyze_tx_logs.sh`
- **Source code:** `aprs_hello_world_clean_app.c`

---

## 💡 Pro Tips

### Run multiple tasks in sequence:

```
1. Task: "Build APRS App"
2. Wait for completion
3. Task: "Log APRS TX Details"
4. Let it run 2 minutes
```

### Compare before/after:

```bash
# After each fix, run same tasks
diff aprs_tx_detailed_BEFORE.log aprs_tx_detailed_AFTER.log
# Watch byte counts, timing, structure changes
```

### Extract just TX data from logs:

```bash
grep "TX\|transmit\|432" aprs_tx_detailed_*.log | head -50
```

### Monitor radio while TX:

```
Terminal 1: Task → "Log APRS TX Details"
Terminal 2: Radio on 432.500 MHz (USB/LSB)
Watch both simultaneously
```

---

**Next step:** Run the "Log APRS TX Details (Extended Debug)" task and review the output in `PROBLEM_ANALYSIS_TX_INTERRUPTION.md`

