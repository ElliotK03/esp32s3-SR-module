# Lock-In

A smart focus device built on ESP32-S3 — touchscreen timer, voice control, phone locker box, and cloud-backed usage analytics.

---

## Overview

Lock-In is a physical productivity device that helps you stay off your phone during focused work. It physically locks your phone in a connected box for the duration of a session, uses voice commands for hands-free control, and syncs session data to Firebase so a nightly ML pipeline can identify your personal focus patterns over time.

---

## Hardware

| Component | Role | Details |
|---|---|---|
| ESP32-S3 | MCU | Dual-core Xtensa LX7, PSRAM, Wi-Fi |
| ST7789 TFT | Display | SPI, driven via custom DMA bounce buffer |
| Capacitive touch panel | Input | I2C, interrupt-driven |
| I2S MEMS microphone | Voice input | Fed into ESP-SR AFE pipeline |
| H-bridge motor driver | Phone locker | MCPWM, 10 kHz PWM, 50% duty cycle |
| INA226 current sensor | Overcurrent protection | I2C, stops motor at 32 mA |

---

## Features

### Phone Locker Box
A DC motor lock/unlock mechanism attached to a physical phone box. When a work session starts, the motor locks the box. It unlocks automatically when the session ends or is stopped. Overcurrent detection via INA226 prevents mechanical binding from stalling the motor.

### Touchscreen UI
Three-screen [LVGL](https://lvgl.io) interface designed in [EEZ Studio](https://www.envox.eu/eez-studio/):
- **Clock screen** — ambient clock with idle timeout
- **Timer screen** — work/break countdown with period selection
- **Settings screen** — volume, brightness, voice toggle, WiFi pairing

Swipe left/right to navigate. Brightness ramps smoothly. All UI updates from background tasks go through `lv_async_call`.

### Voice Control
Fully offline wake word + speech command recognition using [ESP-SR](https://github.com/espressif/esp-sr) (AFE + WakeNet + MultiNet). No cloud inference required. Commands are handled in `main/speech_commands_action.c`. See [docs/voice-recognition.md](docs/voice-recognition.md).

### Pomodoro Timer
Configurable work/break periods with multi-round session tracking. Each completed session records per-round work and rest durations, then pushes a session document to Firestore.

### Cloud Sync
Settings (volume, brightness, lock state) sync bidirectionally with Firebase Firestore via REST — no Firebase SDK on device. An Android companion app can control the device remotely. Conflict resolution uses a monotonic `sync_count` field. See [docs/firestore-architecture.md](docs/firestore-architecture.md).

### Usage Analytics — K-means Pipeline
A Python Cloud Function runs nightly, fetches the last 30 days of session data from Firestore, and clusters sessions by time-of-day using K-means with silhouette-scored K selection (K = 2–4). Output: per-cluster stats (avg work time, rounds, start time) labeled by period (morning / afternoon / evening / night). Results are written back to Firestore and viewable in a local Flask dashboard.

---

## Repository Structure

```
├── main/                    # ESP-IDF application source
│   ├── main.c               # Boot sequence
│   ├── app_logic.c          # Pomodoro state machine + locker sequence
│   ├── connections.c        # WiFi provisioning + Firebase REST sync
│   ├── settings_manager.c   # NVS-backed user settings (single source of truth)
│   ├── motor_control.c      # MCPWM H-bridge driver
│   ├── audio-sr.c           # Wake word + command recognition
│   └── ...
├── components/
│   ├── lockin_capstone/     # LVGL screens (EEZ Studio generated)
│   ├── hardware_driver/     # Board HAL
│   ├── player/              # TTS audio player
│   └── sr_ringbuf/          # Audio ring buffer for AFE
├── cloud/
│   ├── functions/           # K-means ML pipeline (Firebase Cloud Functions, Python)
│   ├── dashboard/           # Local dev Flask dashboard
│   └── scheduler/           # Daily pipeline trigger
├── ml_data_gen/             # Synthetic session data generator for testing
└── docs/                    # Architecture documentation
```

---

## Getting Started

### Prerequisites

- [ESP-IDF v5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/get-started/index.html)
- ESP32-S3 target board

### Build and Flash

```sh
idf.py set-target esp32s3
idf.py menuconfig   # set wake word, audio board, WiFi credentials
idf.py flash monitor
```

WiFi credentials can also be provisioned at runtime from the Settings screen (AP pairing mode).

### Cloud Pipeline — Local Dev

```sh
# Install dependencies
pip install -r cloud/functions/requirements.txt
pip install -r cloud/dashboard/requirements.txt
pip install -r cloud/scheduler/requirements.txt

# Start all services
cd cloud
chmod +x start_dev.sh
./start_dev.sh              # pipeline :8080, dashboard :5000, daily scheduler
./start_dev.sh --no-sched   # skip scheduler (trigger manually via dashboard)
```

Trigger the pipeline manually:

```sh
curl -X POST http://localhost:8080/
```

---

## Documentation

- [Firmware Architecture](docs/firmware-architecture.md) — boot sequence, task map, RAM layout, state machine
- [Firestore Architecture](docs/firestore-architecture.md) — document schema, sync algorithm, session push
- [LVGL Data Flow](docs/lvgl-data-flow.md) — screen navigation, draw pipeline, cross-task UI updates
- [Voice Recognition](docs/voice-recognition.md) — wake word setup, detection threshold, mic sensitivity

---

## Tech Stack

- **Firmware** — [ESP-IDF v5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/), [LVGL v8](https://lvgl.io), [ESP-SR](https://github.com/espressif/esp-sr), FreeRTOS
- **UI design** — [EEZ Studio](https://www.envox.eu/eez-studio/)
- **Cloud** — [Firebase Firestore](https://firebase.google.com/docs/firestore), [Firebase Cloud Functions](https://firebase.google.com/docs/functions) (Python 2nd gen)
- **ML** — scikit-learn KMeans + silhouette scoring, pytz for timezone-aware clustering
