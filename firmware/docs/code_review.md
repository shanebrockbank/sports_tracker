# Code Review + Git Commit Protocol

_Triggered by the user saying "code review". Run this checklist against all files modified this work block, report results, then wait for "git commit" before staging anything._

---

## Step 1 — Build Check
```bash
idf.py build
```
Report: clean / warnings / errors. Do not proceed to checklist if there are errors.

## Step 2 — Code Quality Checklist

Run through each item against the modified files. Report any failures with the file and line number.

### Architecture
- [ ] No direct register access outside a component's own `.c` file
- [ ] No task reads or writes `g_state` fields directly — only via `state_get_*` / `state_set_*`
- [ ] No task calls `system_set_state()` except through the state manager
- [ ] `display_task` is the only task calling any draw or SPI function

### FreeRTOS
- [ ] No `vTaskDelay` used as synchronisation substitute
- [ ] No mutex held across slow operations (SD write, BLE tx, printf, fwrite)
- [ ] Any new task has explicit core assignment, priority, and stack size
- [ ] Any new task has a `uxTaskGetStackHighWaterMark` log at init

### Power
- [ ] Any new peripheral has a power gate or sleep path defined
- [ ] New feature power impact noted (even if "negligible")

### Code style
- [ ] C only — no C++
- [ ] No magic numbers inline — in `#define` or `menuconfig`
- [ ] All sensor data in SI units — conversion only at display layer
- [ ] No unused variables or includes

### Data flow
- [ ] Sensor → display path uses shared state, not a queue
- [ ] Sensor → storage path uses a queue, not shared state
- [ ] SD writes are batched — no per-record `fflush`

---

## Step 3 — Review Report
After running the checklist, report to the user:
- Pass / fail summary
- Any failures with file + line reference
- Any items that could not be verified (e.g. runtime behaviour)
- One sentence on overall code health

Wait for user to respond before proceeding to commit.

---

## Step 4 — Git Commit (triggered by "git commit")

### Commit message format
Follow conventional commits. Scope is the component name.

```
<type>(<scope>): <short description>

[optional body — what and why, not how]
```

**Types:**
- `feat` — new functionality
- `fix` — bug fix
- `docs` — documentation only
- `refactor` — restructure without behaviour change
- `test` — test additions
- `chore` — build, config, tooling

**Examples:**
```
feat(imu): add ICM-20948 accel and gyro initialisation
fix(gps): resolve UART conflict on GPIO16 with SPI bus
docs(architecture): update task table with fusion_task entry
refactor(display): extract draw_status_bar into own function
feat(power): add peripheral gate for GPS via MOSFET GPIO
```

### Commit procedure
1. Run `git diff --stat` and show the user which files will be staged
2. Propose the commit message
3. Wait for user approval or edits
4. On approval: `git add <files>` then `git commit -m "<message>"`
5. Confirm commit hash to user

**Never force push. Never commit to main directly if a feature branch exists.**
