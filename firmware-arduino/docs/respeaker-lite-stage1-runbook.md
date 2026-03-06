# ReSpeaker Lite Stage 1 Runbook

Last updated: 2026-03-04 (PST)

## Goal

Get Elato firmware running on ReSpeaker Lite with:
- mic capture working,
- speaker playback working,
- websocket session flow working,
- no I2S contention regressions.

## Preconditions

- Board connected via XIAO USB serial to `/dev/cu.usbmodem2101` for flash/monitor.
- ReSpeaker XMOS USB-C (near 3.5mm jack) available for DFU checks/flash when needed.
- Branch: `codex/respeaker-lite-stage1`.

## Stage 1 command set

### 1) Verify serial target

```bash
ls /dev/cu.*
```

Expected: `/dev/cu.usbmodem2101` visible.

### 2) Verify XMOS DFU visibility

```bash
dfu-util -l
```

Expected: entries for VID:PID `2886:0019` and DFU alt interfaces (`FACTORY`, `UPGRADE`, `DATAPARTITION`).

### 3) Optional firmware flash (if mismatch or uncertain mode)

Use official I2S firmware file and flash via XMOS USB-C path.

```bash
dfu-util -R -e -a 1 -D <path/to/respeaker_lite_i2s_firmware.bin>
```

Post-step:
- power-cycle board,
- run `dfu-util -l` again,
- record result in this file.

### 4) Build ReSpeaker env

```bash
~/.platformio/penv/bin/platformio run -e xiao_esp32s3_reSpeaker
```

### 5) Upload ReSpeaker env

```bash
~/.platformio/penv/bin/platformio run -e xiao_esp32s3_reSpeaker -t upload
```

Use standard esptool defaults with this branch:
- `upload_speed = 115200`
- no forced `--no-stub`
- no fixed `upload_port` in `platformio.ini` (let PlatformIO auto-detect changing `/dev/cu.usbmodem*` paths)

Also ensure no other serial monitor process is holding the active modem device.

### 6) Monitor boot/runtime logs

```bash
~/.platformio/penv/bin/platformio device monitor -p /dev/cu.usbmodem2101 -b 115200
```

## Validation checklist

- [ ] Boot succeeds with no I2S init error.
- [ ] Websocket connects in ELATO mode.
- [ ] Session transitions observed (`LISTENING -> PROCESSING -> SPEAKING -> LISTENING`).
- [ ] Mic stream is active (non-zero packets/expected behavior).
- [ ] Speaker output is audible.
- [ ] No repeated I2S install/uninstall error loops.

## Recovery playbook

- No mic data:
  - verify XMOS firmware is I2S mode,
  - re-check shared I2S pins.
- No speaker audio:
  - verify DOUT mapping,
  - test volume/gain path and cable/speaker path.
- Bus contention symptoms:
  - keep TX and RX as I2S slaves on shared XMOS clocks,
  - verify ReSpeaker I2S pins (`BCLK=8`, `WS=7`, `DOUT=43`, `DIN=44`).

## Pre-OTA Safety Gates

- Confirm OTA URL/certificate are configured (not placeholder values).
- Confirm build passes and firmware size fits `app0/app1` (2 MB each).
- Confirm `otaState` clears to `OTA_IDLE` on OTA failure (no reboot loop).
- Confirm OTA completion callback failure does not trigger forced reboot loop.
- Confirm debug verbosity is low enough to avoid JWT/token leakage in serial logs.

## Execution record

### XMOS DFU check

- Status: complete
- Output summary: `dfu-util -l` now reports `Found DFU: [2886:0019]` with alt interfaces (`FACTORY`, `UPGRADE`, `DATAPARTITION`), device version `ver=0110`.

### XMOS firmware flash

- Status: complete
- Firmware file: `docs/firmware/respeaker_lite_i2s_dfu_firmware_48k_v1.1.0_ch0-asr_ch1-mww.bin`
- SHA256: `0f4aca05631ea28c1e944c59ed547963728c13224a128c747a38978ec10d4aca`
- Command: `dfu-util -R -e -a 1 -D docs/firmware/respeaker_lite_i2s_dfu_firmware_48k_v1.1.0_ch0-asr_ch1-mww.bin`
- Output summary: transfer completed `100% 274432 bytes`, DFU state returned to `dfuIDLE`, reset to run-time mode requested.

### Build `xiao_esp32s3_reSpeaker`

- Status: complete
- Output summary: success (`platformio run -e xiao_esp32s3_reSpeaker`) after restoring the original Opus->Volume->I2S pipeline for ReSpeaker (`RAM 16.3%`, `Flash 58.3%`).

### Upload `xiao_esp32s3_reSpeaker`

- Status: complete
- Output summary: initial attempts failed during stub handshake (`No serial data received`), then succeeded after enforcing `upload_speed=115200` and `--no-stub`; hash verified, hard reset issued.

### Runtime validation

- Status: partial complete
- Output summary: runtime audio quality validation is pending user listening tests on the rollback-stabilized firmware; prior 48k path produced `opus-decode: buffer too small` instability under live stream.

### Firmware cache for rollback readiness

- Added fallback XMOS image: `docs/firmware/respeaker_lite_i2s_dfu_firmware_v1.0.9.bin`
- SHA256: `3d75a053761fdfd6fb1348fd57a1b7509ca6a2615f60853101da2d11a960c4b9`
- Purpose: quick non-48k XMOS rollback if the 48k image remains incompatible with the current backend audio profile.

### 2026-03-04 stability audit hardening

- `platformio.ini`: removed forced serial upload overrides (`--no-stub`, fixed modem path), normalized to `upload_speed=115200`.
- `platformio.ini`: lowered debug verbosity to `CORE_DEBUG_LEVEL=1` and removed explicit debug port define to reduce websocket header/token logging noise.
- `OTA.cpp` + `WifiManager.cpp`: added OTA config validation and state-machine guardrails so failed OTA/callback paths clear to `OTA_IDLE` instead of reboot looping.
- `Audio.cpp`: OTA auth flags now check OTA config validity before scheduling reboot into OTA mode.
- `Config.cpp`: set ReSpeaker playback sample rate default back to `24000` to stay aligned with the previously proven remote decode/playback profile; explicit override flag remains available for XMOS-matching tests.
- `Audio.cpp`: added startup TX/RX config telemetry lines to verify active I2S runtime parameters during bring-up.
- `Audio.cpp`: added XMOS I2C startup probe logging (`firmware version` + `mute status`) to confirm XMOS control path is alive.
