# Firmware Architecture

ESP-IDF v5.3.2 · ESP32-S3 · FreeRTOS

---

## Boot Sequence (`main/main.c`)

```
app_main()
  ├─ nvs_flash_init()              (connections_init does this, called here first)
  ├─ settings_manager_init()       Load NVS → apply voice/pomodoro settings
  ├─ motor_init()                  MCPWM H-bridge setup
  ├─ ina226_init()                 I2C current sensor at 0x40
  ├─ backlight_init()              PWM backlight GPIO
  ├─ display init (st7789)         SPI display + LVGL driver
  ├─ lvgl_init() / ui_init()       EEZ Studio-generated screens
  ├─ app_logic_init()              Register callbacks, init pomodoro state
  ├─ swipe_init()                  Gesture + ambient clock setup
  ├─ audio_sr_init()               Wake-word + multinet speech recognition
  └─ xTaskCreate(connection_task)  WiFi + Firebase (PSRAM stack)
```

The LVGL tick and handler run in the main task loop (or a pinned LVGL task, depending on config). All UI mutations from other tasks must go through `lv_async_call()`.

---

## Task Map

| Task | Core | Stack | Priority | Notes |
|---|---|---|---|---|
| `connection_task` | any | 8KB PSRAM | 3 | WiFi + Firebase loop |
| `firebase_task` | any | 5KB PSRAM | 2 | Diagnostics + settings sync |
| `session_push_task` | any | 4KB PSRAM | 5 | One-shot per session end |
| `pomo_worker_task` | any | 4KB PSRAM | 5 | Deferred pomo actions |
| `locker_seq` | any | 4KB PSRAM | 5 | Unlock→wait→lock→start sequence |
| `motor_run_timer_task` | any | 4KB internal | 2 | MCPWM motor run + OC detection |
| `detect_Task` | core 0 | 3KB | 5 | Multinet command detection |
| `feed_Task` | core 1 | 2KB | 5 | AFE audio feed |
| NVS write tasks | any | 2KB internal | 5 | `write_nvs_*_task`, must be internal RAM |

**PSRAM rule:** Tasks that do NVS writes must have **internal RAM stacks** because NVS disables the SPI cache during flash writes. All other long-running tasks use PSRAM stacks to preserve ~36KB of internal RAM.

---

## Display Pipeline

```
LVGL draw buffer (PSRAM)
    ↓  lv_display_flush_cb
st7789_flush()
    ↓  spi_master_write_byte()   ← bounce buffer (4092B DRAM)
SPI DMA → ST7789 display
```

**SPI DMA / PSRAM limitation:** On ESP32-S3, SPI DMA cannot read from PSRAM. The `s_spi_bounce_buf[4092]` (DRAM_ATTR) in `st7789.c` copies chunks from PSRAM draw buffers before each DMA transfer, eliminating display glitches without moving the draw buffer to internal RAM (which would cost ~19KB).

### `spi_master_write_color` / `spi_master_write_colors`

These use stack-local `uint8_t Byte[1024]` (not `static`), saving 2048 bytes of static RAM versus the original static arrays.

---

## LVGL Screen Navigation

Screens are defined in EEZ Studio and generated into `components/lockin_capstone/src/ui/`. The screen order for swipe navigation is:

```
[Clock] ← swipe right/left → [Main] ← swipe right/left → [Settings]
```

`screen_swipe.c` manages:
- Swipe gesture detection (`gesture_event_cb`)
- Idle timeout → ambient clock mode
- Brightness ramping (smooth, 3%/40ms)
- Hint label cycling on the clock screen

Screen transitions always go through `loadScreen()` (EEZ Studio generated) to ensure LVGL screen lifecycle is respected.

---

## App Logic State Machine (`main/app_logic.c`)

### Pomodoro States

```
IDLE → WORKING → RESTING → IDLE (loop)
           ↓ (session end or manual stop)
         IDLE
```

State is held in `pomodoro_state_t pomodoro` (file-static). Transitions are driven by `timer_callback()` (FreeRTOS software timer, 1s period).

### Key Functions

| Function | Description |
|---|---|
| `launch_work_session(duration_sec)` | Checks locker_box_connected; spawns locker sequence task or calls `start_timer()` directly |
| `app_logic_start_work_session()` | Public wrapper used by voice commands |
| `start_timer()` | Activates WORKING mode, starts FreeRTOS timer |
| `stop_timer()` | Stops timer, pushes session to Firestore |
| `start_resting_timer(sec)` | Transitions to RESTING mode |
| `is_timer_running()` | Returns `pomodoro.running` |
| `is_timer_working()` | Returns `running && mode == WORKING` |

### Locker Box Sequence

When `locker_box_connected == true` and a work session starts:

1. `g_unlock_cb()` — motor unlock
2. LVGL async: flash "Insert phone" on screen for 5s (400ms toggle × 12)
3. `g_lock_cb()` — motor lock
4. LVGL async: `start_timer()` — begin pomodoro

On session end or manual termination: `g_unlock_cb()` is called before showing the "Take your phone!" / stopping the timer.

### Session Telemetry

Every completed (or early-stopped) session is recorded in a PSRAM-allocated `round_info_t` array. At `stop_timer()`:
- Compute totals/averages
- Build rounds string
- Call `connections_push_session()` → spawns async push task

---

## Settings Manager (`main/settings_manager.c`)

Single source of truth for all user preferences. Persists to NVS via `nvs_helper`.

```
settings_manager_set(new_settings)
  ├─ write_settings_to_nvs()
  ├─ cached_settings = *new_settings
  ├─ apply_voice_settings()   → set_output_vol(), multinet threshold
  ├─ apply_pomodoro_settings() → app_logic_set_work_duration()
  └─ s_sync_count++, s_needs_cloud_push = true
```

Cloud sync (`settings_manager_apply_from_cloud`) does the same NVS write + apply but does **not** increment `sync_count` and clears the push flag.

**NVS write from PSRAM stacks:** `settings_manager_set` is safe to call from any context because the NVS write happens synchronously on the calling task. But if the calling task has a PSRAM stack, the write will panic during cache disable. The pattern used is: cache a `DRAM_ATTR` value, then spawn a small internal-stack task that reads from DRAM and calls NVS. See `write_nvs_manual_override_task` in `app_logic.c` for the reference implementation.

---

## Connections (`main/connections.c`)

### WiFi flow

```
connections_init() [runs as connection_task]
  ├─ nvs_flash_init + load credentials
  ├─ if credentials: connect_saved_wifi_loop()
  │     ├─ connect_to_wifi_once() → scan → connect → wait event
  │     └─ on connected: start_network_services() → sync_time() + firebase_task
  └─ else: wait for START_PAIRING_BIT
        └─ run_pairing_session() → AP mode + HTTP server → receive ssid/pass → save NVS → retry
```

Pairing is triggered from the Settings screen via `connections_start_pairing()`.

### HTTP pairing server

Three endpoints:
- `GET /status` → `{"status":"ready","device":"PomodoroTimer"}`
- `POST /wifi` → JSON `{ssid, password}` → saves to NVS
- `GET /device_id` → `{"device_id":"<MAC>"}`

---

## Motor Control (`main/motor_control.c`)

MCPWM H-bridge driving a DC motor for the lock mechanism.

- PWM frequency: 10 kHz (80 MHz / 8000 ticks)
- Duty cycle: 50%
- Direction: CW = lock, CCW = unlock
- Max run time: 1500ms with 200ms current sampling
- Overcurrent threshold: 32 mA (INA226 at 0x40 via I2C)
- Dead-time: 50 ticks on posedge to prevent shoot-through

`motor_lock()` / `motor_unlock()` spawn `motor_run_timer_task` (internal RAM stack, not PSRAM — motor task is short-lived and doesn't need large allocation).

---

## Voice Speech Recognition (`main/audio-sr.c`)

Uses ESP-SR (AFE + WakeNet + MultiNet):

```
feed_Task (core 1) → esp_board I2S → afe_handle->feed()
detect_Task (core 0) → afe_handle->fetch()
    ├─ WAKENET_DETECTED → play wake chime + lv_async_call(clock_ambient_brighten)
    └─ wakeup_flag=1 → multinet->detect()
          ├─ DETECTED → play ack chime + speech_commands_action(command_id)
          └─ TIMEOUT  → re-arm WakeNet
```

The voice module can be enabled/disabled at runtime via `voice_module_set_enabled()`, which suspends/resumes both tasks and suspends the codec. This is wired to the settings toggle on the UI.

---

## RAM Architecture

Internal RAM budget is tight (~300KB total, ~36KB freed by optimizations).

Key allocations:
- SPI DMA bounce buffer: 4092B DRAM (required — DMA can't use PSRAM)
- LVGL draw buffer: PSRAM (2 × partial buffers)
- Task stacks: PSRAM for all long-running tasks except NVS writers and motor task
- Session rounds array: PSRAM (`heap_caps_malloc(MALLOC_CAP_SPIRAM)`)
- HTTP response buffers: PSRAM (`heap_caps_realloc(..., MALLOC_CAP_SPIRAM)`)
- Static char arrays in `connections.c`: shrunk (diagnostics body 512→320, URLs 512→200/256/320)

Diagnostics to monitor at runtime: `free_heap_bytes` in the Firestore heartbeat document.
