# Voice Recognition

The voice module uses [Espressif ESP-SR](https://github.com/espressif/esp-sr) for fully offline, on-device wake word and speech command detection (no cloud inference required).

---

## Pipeline

```
feed_Task (core 1) ── I2S MEMS mic ──► afe_handle->feed()
detect_Task (core 0) ◄── afe_handle->fetch()
    ├── WAKENET_DETECTED  → play wake chime + brighten display
    └── wakeup_flag = 1   → multinet->detect()
            ├── DETECTED  → play ack chime + speech_commands_action(command_id)
            └── TIMEOUT   → re-arm WakeNet
```

Both tasks are pinned to separate cores to avoid starving the audio feed. The module can be toggled at runtime from the Settings screen or via `voice_module_set_enabled(bool)`, which suspends both tasks and the codec without a reboot.

---

## Wake Word Selection

Wake words are selected at build time via `idf.py menuconfig`:

```sh
idf.py menuconfig

# Single wake word
ESP Speech Recognition → Select wake words → Hi,Lexin (wn9_hilexin)

# Multiple wake words (up to two active simultaneously)
ESP Speech Recognition → Select wake words → Hi,Lexin → Load Multiple Wake Words
ESP Speech Recognition → Load Multiple Wake Words → Hi,Lexin (wn9_hilexin)
                                                   → Hi,ESP (wn9_hiesp)
```

Available models: [esp-sr/model/wakenet_model](https://github.com/espressif/esp-sr/tree/master/model/wakenet_model)

---

## Detection Threshold

Each model ships with a default threshold range printed to serial at init. Example:

```
wakenet9_v1h24_嗨，乐鑫_3_0.608_0.615  →  threshold range: 0.608 – 0.615
```

**Adjust at runtime (persisted to NVS):**

```c
settings_manager_set_wakenet_threshold(0.65f);
```

**Adjust via AFE handle directly (not persisted):**

```c
afe_handle->set_wakenet_threshold(afe_handle, model_index, threshold);
afe_handle->reset_wakenet_threshold(afe_handle, model_index); // restore default
```

> Threshold adjustments via the settings manager take effect immediately via `multinet_set_detection_threshold()`. Changes via the AFE handle require a reboot to persist.

---

## Microphone Sensitivity

Mic gain is configured at boot in `main/main.c`:

```c
set_input_sensitivity(10);  // range 0–30; level 6 ≈ 30 dB gain
```

Raise the value if the wake word is frequently missed. Lower it if false triggers occur in noisy environments.

---

## Command Recognition Timeout

After a wake word is detected, MultiNet listens for a command for a configurable window:

```c
set_command_recognition_timeout(5000); // milliseconds, stored in NVS
```

Changes are stored in NVS and take effect after reboot.

---

## References

- [ESP-SR GitHub](https://github.com/espressif/esp-sr)
- [ESP-SR API reference](https://docs.espressif.com/projects/esp-sr/en/latest/)
- [WakeNet model list](https://github.com/espressif/esp-sr/tree/master/model/wakenet_model)
- [MultiNet command format](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/multinet/README.html)
