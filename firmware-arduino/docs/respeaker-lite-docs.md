# ReSpeaker Lite Documentation Pack

Last updated: 2026-03-04 (PST)

## Official references

- ReSpeaker Lite repo (official): https://github.com/respeaker/ReSpeaker_Lite
- ReSpeaker Lite README (raw): https://raw.githubusercontent.com/respeaker/ReSpeaker_Lite/master/README.md
- XMOS DFU guide (raw): https://raw.githubusercontent.com/respeaker/ReSpeaker_Lite/master/xmos_firmwares/dfu_guide.md
- XMOS firmware changelog (raw): https://raw.githubusercontent.com/respeaker/ReSpeaker_Lite/master/xmos_firmwares/changelog.md
- XMOS firmware folder: https://github.com/respeaker/ReSpeaker_Lite/tree/master/xmos_firmwares
- Seeed getting started (reSpeaker Lite): https://wiki.seeedstudio.com/reSpeaker_usb_v3/
- Seeed voice assistant kit page: https://wiki.seeedstudio.com/xiao_respeaker/
- Seeed record/play reference (I2S baseline): https://wiki.seeedstudio.com/respeaker_record_and_play/
- ESPHome micro wake word docs: https://esphome.io/components/micro_wake_word.html
- ESPHome Voice Kit implementation (XMOS/I2C controls): https://raw.githubusercontent.com/esphome/home-assistant-voice-pe/dev/esphome/components/voice_kit/voice_kit.cpp

## Stage 1 integration facts (firmware-relevant)

- ReSpeaker Lite has two firmware families:
  - USB firmware (acts as USB sound card)
  - I2S firmware (required for host-MCU integration with XIAO ESP32S3)
- XMOS DFU transport:
  - USB VID/PID commonly shown as `2886:0019`
  - Flash command: `dfu-util -R -e -a 1 -D <firmware.bin>`
- ReSpeaker Lite I2C details from vendor docs:
  - I2C address: `0x42` (XMOS command/config transport)
  - XIAO pins on kit: `D4 (SDA = GPIO5)`, `D5 (SCL = GPIO6)`

## Stage 1 host pin mapping (chosen)

This firmware stage uses a dedicated ReSpeaker board profile with these I2S pins:

- Shared clocks:
  - BCLK: `GPIO8`
  - WS/LRCLK: `GPIO7`
- I2S TX from ESP32 to ReSpeaker codec/XMOS:
  - DOUT: `GPIO43`
- I2S RX into ESP32 from ReSpeaker mic pipeline:
  - DIN: `GPIO44`

Notes:
- ReSpeaker has no dedicated external amp shutdown pin equivalent to the current `I2S_SD_OUT` behavior in this repo.
- The ReSpeaker profile therefore treats shutdown toggles as optional no-ops.
- ReSpeaker Stage-1 playback rate is now hard-locked to `24 kHz` in code for proven-safe behavior.
- Rate overrides are intentionally disabled in this branch to avoid accidental mismatch regressions.

## Official Record/Play I2S Baseline (Seeed)

From `respeaker_record_and_play` and the related Arduino examples:

- TX pins: `BCLK=8`, `WS=7`, `DOUT=43`, `DIN=44`
- TX mode: `is_master = false` (XMOS drives clocks)
- Example playback format shown by Seeed:
  - `AudioInfo(16000, 2, 32)`
- I2C bus for XMOS/codec control:
  - `Wire.begin(5,6)`

## Firmware targets to keep in mind

From current official changelog and firmware listing:

- I2S firmware stable line: `v1.0.9`
- I2S firmware with micro wake word split-channel behavior: `v1.1.0_ch0-asr_ch1-mww`
- USB firmware stable line: `v2.0.7`

For Stage 1 bring-up, we prioritize an I2S firmware image compatible with standard ASR streaming path.

## User controls and wakeword references

- Board-level controls (from Seeed docs):
  - `USR` button (user defined)
  - `Mute` button + mute indicator
- Wakeword direction references:
  - ESPHome micro wake word runtime/actions: https://esphome.io/components/micro_wake_word.html
  - XMOS + pipeline register control patterns: `voice_kit.cpp` reference above.

## RGB LED and User Button GPIO details (ReSpeaker Lite Voice Assistant Kit)

From Seeed's `xiao_respeaker` docs and examples:

- RGB LED:
  - Kit docs show the on-board RGB is a WS2812-class LED on XIAO `A0`.
  - In this firmware branch, state LED output is routed to a single NeoPixel on `A0` (ReSpeaker env only).
  - Seeed also provides XMOS-side LED test/control flows (`xmos_led_ring`) for their reference firmware path.
- `USR` button:
  - Jumper-selectable routing to XIAO pins:
    - `USR -> D3` means read on ESP32 `GPIO4`,
    - `USR -> D2` means read on ESP32 `GPIO3`.
  - This is board-jumper dependent, not a single hard-wired default GPIO.

Reference pages:
- Kit docs (contains `USR` jumper guidance): https://wiki.seeedstudio.com/xiao_respeaker/
- RGB LED test page (`xmos_led_ring` flow): https://wiki.seeedstudio.com/respeaker_light_rgb_test/
- `USR` button test page (`GPIO4` / `GPIO3` mapping): https://wiki.seeedstudio.com/respeaker_light_button/

### Current firmware status in this repo

- ReSpeaker Stage 1 uses `Adafruit NeoPixel` (single pixel) on `A0` for state LED colors.
- Legacy 3-pin analog RGB placeholders remain disabled (`RED/GREEN/BLUE = -1`) to prevent accidental mixed control paths.
- ReSpeaker `BUTTON_PIN` is set to `GPIO3` for this branch because `USR` is wired to `D2`.
- Button pathway in `main.cpp` is active again (long-press/double-click callbacks enabled when `BUTTON_PIN` is assigned).

## Security and provenance checks

Before flashing any firmware binary:

- Download only from official upstream links above.
- Record hash:

```bash
shasum -a 256 <firmware.bin>
```

- Keep flash action in a runbook entry with:
  - firmware file name
  - hash
  - command used
  - resulting dfu-util output summary

## Method Compliance Check (Volume + Amp Enable)

Checked on: 2026-03-03 (PST)

Official ReSpeaker Arduino example methods:

- Volume control method (`xiao_i2c_control_volume.ino`):
  - `Wire.begin(5,6)`
  - Codec I2C address `0x18` (TLV320AIC3204)
  - Select page 1 (`reg 0x00 = 0x01`) then write output driver gains:
    - `reg 0x10`, `0x11`, `0x12`, `0x13`
- Speaker enable / mute (`xiao_i2c_write_register_value.ino` + README):
  - XMOS I2C address `0x42`
  - Packet: `[RESID, CMD, LEN, VALUE...]`
  - Uses `RESID=0xF1`, `CMD=0x10` to control `AUDIO_PA_EN` (amp path)

Current status in this repo (ReSpeaker env):

- Speaker amp enable now follows official XMOS method over I2C (`0x42`) via `RESID=0xF1`, `CMD=0x10`.
- Runtime output volume currently follows the original project’s digital `VolumeStream` path (same as the known-good Sense branch) to avoid double-gain distortion while stabilizing ReSpeaker playback.
- Legacy GPIO shutdown remains in code for non-ReSpeaker profiles; ReSpeaker uses XMOS `AUDIO_PA_EN` for amp gate control.

## 2026-03-04 Stability Audit Notes

- Serial upload configuration was normalized back to safer defaults (`115200`, no forced `--no-stub`, no fixed `/dev/cu.*` path in config).
- OTA state handling was hardened to prevent reboot loops:
  - invalid OTA URL/certificate placeholders now abort OTA and clear to `OTA_IDLE`,
  - OTA fail path clears `OTA_IDLE` before reset,
  - OTA completion callback failures no longer force repeated restart loops,
  - incoming remote OTA flags are ignored when OTA config is incomplete (no forced reboot).
- Debug verbosity was reduced to lower runtime overhead and prevent bearer token exposure in serial logs.
