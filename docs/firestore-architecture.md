# Firestore Architecture

## Overview

The firmware communicates with Google Firestore via the REST API over HTTPS (no Firebase SDK). All rules are currently open (no authentication required), mirroring the unauthenticated approach used in both the firmware and the `ml_data_gen` Python scripts.

**Project ID:** `lock-in-81e21`

---

## Document Schema

### `/devices/{device_id}`

Top-level device document. `device_id` is the 12-character hex MAC address (e.g. `14C19F442534`).

| Field | Firestore type | Description |
|---|---|---|
| `lock_status` | `stringValue` | `"LOCKED"` or `"UNLOCKED"` |
| `speaker_volume` | `integerValue` | 0–100 |
| `screen_brightness` | `integerValue` | 0–100 |
| `sync_count` | `integerValue` | Conflict resolution counter (see below) |
| `lock_manual_override` | `booleanValue` | Whether manual lock override is active |

The device document is **never deleted**; it is created via `PATCH` with `updateMask` field paths on first push.

### `/devices/{device_id}/sessions/{session_id}`

One document per completed pomodoro session. `session_id` is a UUID v4 generated on device via `esp_random()`.

| Field | Firestore type | Description |
|---|---|---|
| `sesstionStartDate` | `stringValue` | `DD/MM/YYYY` — **firmware typo preserved** |
| `sessionStartTime` | `stringValue` | `HH:MM` (24h) |
| `numberofRounds` | `integerValue` | Count of work+break pairs |
| `rounds` | `stringValue` | Multiline text encoding each round (see below) |
| `totalWorkSec` | `integerValue` | Sum of all work durations |
| `avgWorkSec` | `integerValue` | `totalWorkSec / numberofRounds` |
| `totalRestSec` | `integerValue` | Sum of all break durations |
| `avgRestSec` | `integerValue` | `totalRestSec / numberofRounds` |

### `/diagnostics/{device_id}`

Heartbeat document pushed every 5 seconds while connected.

| Field | Description |
|---|---|
| `timestamp` | Unix epoch ms |
| `blink_count` | Monotonically increasing counter per connection |
| `led_state` | Always `true` (legacy) |
| `wifi_rssi` | Current RSSI in dBm |
| `free_heap_bytes` | `esp_get_free_heap_size()` |
| `uptime_seconds` | `esp_timer_get_time() / 1e6` |

---

## `rounds` String Format

The `rounds` field is a multiline string encoded by the firmware:

```
 \n
        0: {1500, 300}\n
        1: {1500, 0}\n
    
```

Each line is `        {index}: {work_sec, break_sec}`. The last round's `break_sec` is typically `0` (no rest after final round). JSON special characters (`\n`, `"`, `\`) are escaped by `escape_json_string()` before inclusion in the HTTP body.

---

## Sync Count Algorithm

`sync_count` is an integer that acts as a logical clock for conflict resolution between the device and the cloud (e.g. a companion app).

**On device change:** `s_sync_count++` → write to NVS → set `s_needs_cloud_push = true`.

**On cloud poll (`firestore_fetch_device_settings`):**
- If `cloud_count > local_count` → cloud is newer → apply cloud fields, update local sync_count.
- If `cloud_count <= local_count` → local is authoritative → do nothing.

**Push order within one poll cycle:**
1. Push local → cloud (if `s_needs_cloud_push`).
2. Fetch cloud → apply if cloud is newer.

This ensures a local change is never immediately overwritten by a stale cloud value.

---

## Push / Fetch Cycle

The `firebase_task` (PSRAM stack, priority 2) runs in a `while(true)` loop:

1. If not connected: sleep 1s, loop.
2. On first connection: `settings_manager_signal_cloud_push()` to push initial local state.
3. Every 5s:
   a. Push diagnostics (heartbeat).
   b. If `needs_cloud_push`: push device settings via `PATCH` with `updateMask`.
   c. `GET` device document → apply cloud settings if cloud wins.

---

## Session Push

Sessions are pushed asynchronously from a short-lived `session_push_task` (PSRAM stack, 4096 bytes) spawned at the end of a pomodoro session. The caller (`connections_push_session`) allocates a `session_push_data_t` in PSRAM and transfers ownership. The task:

1. Allocates escaped-rounds buffer and body buffer in PSRAM.
2. Escapes the rounds string.
3. Builds the JSON body.
4. `POST`s to `/devices/{id}/sessions?documentId={uuid}`.
5. Frees all allocations, deletes itself.

If the network is not ready when a session ends, the data is **discarded** (no local queue/retry).

---

## Security Model

Firestore rules are currently open — any client can read/write any document. This is intentional for the prototype phase. The device does not use any authentication token.

**Implication:** The `ml_data_gen/generate_sessions.py` script exploits the same open-rules approach to push synthetic training data without needing a service account.

---

## `ml_data_gen/generate_sessions.py`

Generates 100 synthetic pomodoro sessions and pushes them to Firestore for ML training without real user data.

- Device ID: `14C19F442534`
- 1–6 rounds per session, weighted toward 3 rounds
- Work durations: 15/20/25/30/45 min; rest: 5/10/15 min
- ~20% early stops (60–95% of planned work)
- Spread over the past 60 days, 07:00–23:00
- `rounds` string format matches firmware exactly (including `sesstionStartDate` typo)
- 0.3s delay between pushes to respect Firestore free-tier write limits
