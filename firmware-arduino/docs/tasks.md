# ReSpeaker Lite Stage 1 Tasks

[x] 2026-03-01 15:28 PST Create feature branch `codex/respeaker-lite-stage1` (switched successfully).
[x] 2026-03-01 15:30 PST Create and populate ReSpeaker documentation pack in `/docs` (4 docs created).
[x] 2026-03-01 15:32 PST Add `respeaker_lite_stage1` PlatformIO environment.
[x] 2026-03-01 15:32 PST Add ReSpeaker board profile split in `src/Config.h` and `src/Config.cpp`.
[x] 2026-03-01 15:32 PST Add safe no-op handling for speaker shutdown pin on ReSpeaker.
[x] 2026-03-01 15:32 PST Add ReSpeaker RX clocking strategy guard in `src/Audio.cpp`.
[x] 2026-03-01 15:32 PST Verify XMOS DFU visibility and flash I2S firmware if needed (dfu-util available, no DFU device detected on current cable path).
[x] 2026-03-01 15:33 PST Build `respeaker_lite_stage1` (success, RAM 16.0%, Flash 57.4%).
[x] 2026-03-01 15:34 PST Build regression env `xiao_esp32s3_sense`.
[x] 2026-03-01 15:36 PST Upload `respeaker_lite_stage1` to `/dev/cu.usbmodem2101` (flash + verify successful).
[x] 2026-03-01 15:36 PST Monitor serial logs and validate stage-1 behavior (boot/runtime logs seen; no I2S crash traces, SoftAP heartbeat active).
[x] 2026-03-01 15:36 PST Write Stage 2 wakeword/interrupt design note (XMOS-first).
[x] 2026-03-03 20:33 PST Validate explicit board-profile split after review (ReSpeaker and XIAO Sense builds both pass: RAM/Flash 16.0%/57.4% and 16.2%/58.2%).
[x] 2026-03-03 20:37 PST Fully separate PlatformIO envs for `xiao_esp32s3_sense` and `respeaker_lite_stage1` (removed inheritance coupling between board profiles).
[x] 2026-03-03 20:38 PST Stabilize ReSpeaker upload path with `upload_speed=115200` + `upload_flags=--no-stub` (flash/write/hash verify succeeded).
[x] 2026-03-03 20:38 PST Validate post-flash runtime via serial monitor (`/dev/cu.usbmodem2101`) with active Wi-Fi status log.
[x] 2026-03-03 20:44 PST Document ReSpeaker RGB/USR GPIO behavior from Seeed sources (RGB via XMOS command path; USR jumper-routed to GPIO3/GPIO4).
[x] 2026-03-03 20:44 PST Align ReSpeaker firmware config: unassign direct RGB GPIO pins and default `BUTTON_PIN` to `GPIO_NUM_NC` for jumper-based routing.
[x] 2026-03-03 20:49 PST Rename branch env to `xiao_esp32s3_reSpeaker` and remove Sense envs from this ReSpeaker-only branch.
[x] 2026-03-03 20:50 PST Validate renamed env build (`platformio run -e xiao_esp32s3_reSpeaker` succeeded; RAM 16.0%, Flash 57.4%).
[x] 2026-03-03 21:02 PST Add runtime telemetry output (heap/min-heap + task stack high-water marks) to main loop for live capacity checks.
[x] 2026-03-03 21:03 PST Add non-blocking generative servo motion method note for I2C PWM controller path (`ServoMCU/Docs/servo-generative-motion-method.md`).
[x] 2026-03-03 22:23 PST Flash telemetry-enabled ReSpeaker build and verify runtime telemetry line appears in serial output while websocket/audio are active.
[x] 2026-03-03 22:29 PST Patch ReSpeaker LED backend to WS2812 on `A0` using lightweight `Adafruit NeoPixel` and route state colors through this path.
[x] 2026-03-03 22:29 PST Validate WS2812 LED build in `xiao_esp32s3_reSpeaker` (RAM 16.1%, Flash 57.9%).
[x] 2026-03-03 22:34 PST Reactivate ReSpeaker button pathway with `USR -> D2` (`BUTTON_PIN=GPIO3`) and re-enable button callback setup in `main.cpp`.
[x] 2026-03-03 23:13 PST Implement official ReSpeaker I2C methods for volume (AIC3204) and amp enable (`XMOS 0xF1/0x10`) in ReSpeaker board path; build+flash to `/dev/cu.usbmodem2101` succeeded (RAM 16.3%, Flash 59.4%).
[x] 2026-03-04 03:45 PST Investigate static-burst issue from runtime logs; applied ReSpeaker-safe playback stabilizers (I2S TX slave clock + temporary pitch-shift bypass), build passed (RAM 16.3%, Flash 59.5%).
[x] 2026-03-04 12:09 PST Perform in-depth firmware stability audit before OTA retry (completed code+config+docs pass, fresh build successful).
[x] 2026-03-04 12:09 PST Harden OTA failure handling to prevent reboot loops after failed OTA or backend callback failures (added OTA config validation + idle-state recovery path).
[x] 2026-03-04 12:09 PST Remove high-verbosity debug flags that leak bearer tokens and can destabilize real-time audio timing (`CORE_DEBUG_LEVEL=1`, removed explicit debug serial macro).
[x] 2026-03-04 12:09 PST Re-align PlatformIO serial upload defaults to safe, non-experimental settings for recovery flashing fallback (removed forced `--no-stub` and fixed modem path).
[x] 2026-03-04 12:09 PST Document audit findings + remediation checklist in ReSpeaker docs pack (`respeaker-lite-docs.md` + `respeaker-lite-stage1-runbook.md` updated).
[x] 2026-03-04 12:09 PST Rebuild hardened ReSpeaker firmware env (`xiao_esp32s3_reSpeaker`) after audit patches (RAM 16.3%, Flash 58.2%).
[x] 2026-03-04 12:10 PST Add XMOS startup diagnostics (firmware version + mute status via I2C) to help verify ReSpeaker firmware state before OTA rollouts.
[x] 2026-03-04 12:12 PST Add guard to ignore OTA reboot requests when OTA URL/certificate config is incomplete (prevents unnecessary restart on remote `is_ota=true` flags).
[x] 2026-03-04 12:12 PST Rebuild after OTA-guard changes (`xiao_esp32s3_reSpeaker` success, RAM 16.3%, Flash 58.3%).
[x] 2026-03-04 12:19 PST Push OTA update to `192.168.1.166:3232` with `espota.py` using current `xiao_esp32s3_reSpeaker` firmware binary (transfer reached 100%).
[x] 2026-03-04 12:19 PST Post-OTA checks: device responded on network and re-enumerated as `/dev/cu.usbmodem101`; TCP probe on `3232` now refused (expected after replacing temporary OTA sketch).
[x] 2026-03-04 12:25 PST Rebuild with ReSpeaker audio-rate tuning patch (`SAMPLE_RATE` configurable, default set to 24k for TalkEdge path).
[x] 2026-03-04 12:25 PST OTA retry attempt blocked at invitation phase (`espota.py` timed out with listener inactive), indicating temporary ArduinoOTA helper sketch was not running on target at that moment.
[x] 2026-03-04 12:31 PST Implement Option 1 OTA alignment: replace placeholder OTA URL/cert in `src/OTA.cpp` with build-driven native HttpsOTA config (`ELATO_OTA_FIRMWARE_URL`, optional `ELATO_OTA_SERVER_CERT_PEM`).
[x] 2026-03-04 12:31 PST Add OTA config knobs to `platformio.ini` for ReSpeaker env and keep defaults safe/off unless explicitly configured.
[x] 2026-03-04 12:31 PST Rebuild `xiao_esp32s3_reSpeaker` after Option 1 OTA patch (success, RAM 16.3%, Flash 58.3%).
[x] 2026-03-04 12:32 PST Attempt cable upload; blocked by missing USB modem in current session (auto-detected `/dev/cu.KDC-BT378U`, ESP connect failed: no serial data received).
[x] 2026-03-04 12:36 PST Retried auto-detect + 30s active poll for `/dev/cu.usbmodem*`; no USB modem node appeared, so cable upload could not start.
[x] 2026-03-04 12:37 PST Auto-detected USB modem `/dev/cu.usbmodem2101` and completed cable upload successfully (full flash + hash verify + RTS reset).
[-] 2026-03-04 12:43 PST Audio distortion remediation pass: switch ReSpeaker TX to single-clock master (RX remains slave) to remove shared-I2S clock mismatch/static artifacts.
[x] 2026-03-04 12:44 PST Build with TX-master/RX-slave ReSpeaker clocking patch succeeded (RAM 16.3%, Flash 58.3%).
[x] 2026-03-04 12:45 PST Upload retries failed due serial transport instability (`chip stopped responding`, `Invalid head of packet`, and `device reports readiness but returned no data` on `/dev/cu.usbmodem2101`).
[x] 2026-03-04 12:49 PST Uploaded TX-master/RX-slave clocking fix successfully to `/dev/cu.usbmodem2101` (full write + hash verify + RTS reset).
[-] 2026-03-04 12:52 PST Re-align remote playback handling to known-good branch: restore digital stream volume + server pitch_factor path (remove ReSpeaker-only pitch override and codec-volume runtime scaling).
[x] 2026-03-04 12:53 PST Re-aligned remote playback handling to known-good branch behavior (digital stream volume + server pitch_factor), built and flashed successfully to `/dev/cu.usbmodem2101`.
[-] 2026-03-04 12:58 PST Apply I2S TX distortion hotfix: force ReSpeaker TX `channel_format=I2S_CHANNEL_FMT_ALL_LEFT` to bypass audio-tools per-sample mono->stereo software expansion path.
[x] 2026-03-04 12:59 PST Applied and flashed I2S TX mono-slot hotfix (`channel_format=ALL_LEFT`) to `/dev/cu.usbmodem2101` (full write + hash verify + RTS reset).
[x] 2026-03-04 13:08 PST Replace ReSpeaker playback sink with native ESP32 I2S stereo writer (`i2s_write`) to eliminate channel/slot mismatch static while keeping Opus/network path unchanged; build passed (`xiao_esp32s3_reSpeaker`, RAM 17.2%, Flash 58.2%).
[x] 2026-03-04 13:09 PST Verify live XMOS firmware state from runtime I2C diagnostics: board reports `v1.0.8` (not `v1.0.9`).
[-] 2026-03-04 13:12 PST Upgrade XMOS I2S firmware to `v1.1.0 48k` (`respeaker_lite_i2s_dfu_firmware_48k_v1.1.0_ch0-asr_ch1-mww.bin`) via DFU before the next audio validation pass.
[x] 2026-03-04 13:13 PST Pulled requested XMOS firmware binary into repo docs cache: `docs/firmware/respeaker_lite_i2s_dfu_firmware_48k_v1.1.0_ch0-asr_ch1-mww.bin` (SHA256 `0f4aca05631ea28c1e944c59ed547963728c13224a128c747a38978ec10d4aca`).
[x] 2026-03-04 13:13 PST Re-ran DFU detection (`dfu-util -l`): still no `2886:0019` DFU interfaces visible; board not yet enumerated in DFU mode.
[x] 2026-03-04 13:24 PST XMOS flashed successfully to `respeaker_lite_i2s_dfu_firmware_48k_v1.1.0_ch0-asr_ch1-mww.bin` using `dfu-util -R -e -a 1 -D ...`; post-flash `dfu-util -l` reports `ver=0110` on `2886:0019`.
[-] 2026-03-04 13:29 PST Align ESP playback to XMOS 48k firmware profile by setting `ELATO_RESPEAKER_AUDIO_RATE=48000` in ReSpeaker env and rebuilding before next upload.
[x] 2026-03-04 13:30 PST Aligned ESP playback to XMOS 48k profile (`ELATO_RESPEAKER_AUDIO_RATE=48000` in `platformio.ini`) and rebuilt `xiao_esp32s3_reSpeaker` successfully (RAM 17.2%, Flash 58.2%).
[-] 2026-03-04 13:32 PST Upload rebuilt 48k-aligned ESP firmware to `/dev/cu.usbmodem2101` and confirm runtime audio init (`[AUDIO][TX]` / `[AUDIO][RX]`) against XMOS v1.1.0.
[x] 2026-03-04 13:32 PST Uploaded rebuilt 48k-aligned ESP firmware to `/dev/cu.usbmodem2101` (full write + hash verify + RTS reset); runtime test revealed `opus-decode: buffer too small` instability under live response stream.
[-] 2026-03-04 13:40 PST Roll back ESP-side 48k override (`ELATO_RESPEAKER_AUDIO_RATE=48000`) and reflash stable decode profile.
[x] 2026-03-04 13:41 PST Rolled back ESP-side 48k override (`ELATO_RESPEAKER_AUDIO_RATE=48000`) after confirming Opus decode regressions under live stream.
[-] 2026-03-04 13:42 PST Re-apply proven-safe serial upload settings (`--no-stub`, 115200) and complete rollback flash to `/dev/cu.usbmodem2101`.
[x] 2026-03-04 13:49 PST Pull official ReSpeaker playback reference (`respeaker_record_and_play`) and align firmware TX clocking to the documented slave-mode method (`is_master=false`, standard I2S format).
[x] 2026-03-04 13:50 PST Remove experimental ReSpeaker-native `i2s_write` playback path and restore the original Sense-proven Opus->Queue->Volume->I2S stream pipeline.
[x] 2026-03-04 13:50 PST Re-enable digital runtime volume/pitch handling on ReSpeaker path to match known-good remote audio behavior from the original project flow.
[x] 2026-03-04 13:50 PST Reset ReSpeaker default `SAMPLE_RATE` back to 24k (with build-flag override still available) to keep server decode/playback alignment stable by default.
[x] 2026-03-04 13:50 PST Build validation passed for `xiao_esp32s3_reSpeaker` after pipeline rollback (RAM 16.3%, Flash 58.3%).
[x] 2026-03-04 13:51 PST Upload succeeded to `/dev/cu.usbmodem2101` after clearing an external serial monitor lock that was holding the port.
[x] 2026-03-04 13:53 PST Downloaded official XMOS fallback firmware `respeaker_lite_i2s_dfu_firmware_v1.0.9.bin` for non-48k rollback readiness (SHA256 `3d75a053761fdfd6fb1348fd57a1b7509ca6a2615f60853101da2d11a960c4b9`).
[x] 2026-03-04 14:05 PST Enforced hard-safe firmware defaults by locking ReSpeaker `SAMPLE_RATE` to `24000` in code (removed rate override path in this branch).
[x] 2026-03-04 14:05 PST Checked XMOS DFU visibility for immediate rollback (`dfu-util -l`): no `2886:0019` device currently visible, so board is not in DFU mode yet.
[x] 2026-03-04 14:11 PST Rolled XMOS back from `v1.1.0 48k` to stable `v1.0.9` using `dfu-util -R -e -a 1 -D docs/firmware/respeaker_lite_i2s_dfu_firmware_v1.0.9.bin`; post-flash verification shows `ver=0109`.
[x] 2026-03-04 14:11 PST Runtime validation item superseded by a stricter post-patch check after TX format conversion and 16k host lock.
[x] 2026-03-04 14:18 PST ReSpeaker audio-format remediation applied: TX path now converts Opus PCM `mono/16-bit` to ReSpeaker wire format `stereo/32-bit` before I2S write.
[x] 2026-03-04 14:19 PST Locked ReSpeaker profile playback baseline to 16k in `Config.cpp` to match official Seeed host examples.
[x] 2026-03-04 14:19 PST Rebuild passed after ReSpeaker format/rate patch (`xiao_esp32s3_reSpeaker`: RAM 16.8%, Flash 60.1%).
[x] 2026-03-04 14:20 PST Reflash succeeded on `/dev/cu.usbmodem2101` after clearing a conflicting serial monitor lock on the USB modem.
[x] 2026-03-04 14:20 PST Runtime validation item superseded by gain-staging patch validation (new pending item at 14:34 covers current test criteria).
[x] 2026-03-04 14:31 PST Distortion/gain remediation applied: ReSpeaker now uses official AIC3204 codec volume writes (`0x18`, page-1 regs `0x10..0x13`), conservative digital attenuation, and pitch-shift bypass for cleaner output.
[x] 2026-03-04 14:33 PST Rebuild passed after gain-staging patch (`xiao_esp32s3_reSpeaker`: RAM 16.8%, Flash 60.0%).
[x] 2026-03-04 14:34 PST Flashed gain-remediated firmware to `/dev/cu.usbmodem2101` (full write + hash verify + reset).
[-] 2026-03-04 14:34 PST Runtime validation pending: check if distortion/fuzz is reduced to acceptable quality with intelligible voice on current hardware.
[x] 2026-03-04 14:40 PST Aggressive anti-overdrive pass applied: further reduced ReSpeaker analog+digital output gain and added codec-write success logging for AIC3204 register writes.
[x] 2026-03-04 14:41 PST Build passed after aggressive low-gain patch (`xiao_esp32s3_reSpeaker`: RAM 16.8%, Flash 60.0%).
[x] 2026-03-04 14:41 PST Flashed aggressive low-gain firmware to `/dev/cu.usbmodem2101` (full write + hash verify + reset).
[-] 2026-03-04 14:41 PST Runtime validation pending: verify whether overdrive distortion is resolved with very low output gain profile.
[x] 2026-03-04 14:47 PST Rolled back aggressive low-gain constants to restore the previous intelligible ReSpeaker profile baseline.
[x] 2026-03-04 14:43 PST Applied deterministic ReSpeaker playback repack (`mono16 -> stereo32`) with direct I2S writes to remove conversion-chain distortion risk.
[x] 2026-03-04 14:44 PST Build passed after deterministic repack patch (`xiao_esp32s3_reSpeaker`: RAM 17.5%, Flash 60.0%).
[x] 2026-03-04 14:45 PST Flashed deterministic repack firmware to `/dev/cu.usbmodem2101` (full write + hash verify + reset).
[-] 2026-03-04 14:45 PST Runtime validation pending: verify speech clarity/distortion after conversion-chain bypass patch on current XMOS v1.0.9 + 16k profile.
[x] 2026-03-04 14:48 PST Patched response-turn race in `Audio.cpp`: cancel pending delayed-listen restart on `RESPONSE.CREATED` / `transitionToSpeaking` to prevent dropped speaker frames.
[x] 2026-03-04 14:48 PST Restored ReSpeaker server-volume semantics (removed extra digital attenuation multiplier) so `volume_control` maps predictably again.
[x] 2026-03-04 14:49 PST Build passed after race + volume fix (`xiao_esp32s3_reSpeaker`: RAM 17.5%, Flash 60.0%).
[x] 2026-03-04 14:52 PST Reflash succeeded to `/dev/cu.usbmodem2101` after clearing stale serial-holder process (`python3.1` on the modem device).
[x] 2026-03-04 14:52 PST Runtime log spot-check confirms no immediate `Skipping audio data due to touch interrupt` burst after `RESPONSE.CREATED`.
[-] 2026-03-04 14:52 PST Runtime validation pending: user listening pass for final speech quality (distortion/clarity) on latest race-fixed build.
[x] 2026-03-05 09:03 PST Instrument ReSpeaker playback path to validate sample-rate pressure (TX queue underrun telemetry) and retune gain staging.
[x] 2026-03-05 09:21 PST Implement explicit ReSpeaker TX upsampling before I2S write (decode PCM -> 48k bus rate) and add byte-alignment safety in queue reader.
[x] 2026-03-05 09:22 PST Build + flash upsampling patch to `/dev/cu.usbmodem2101` (`xiao_esp32s3_reSpeaker`, RAM 20.2%, Flash 60.0%).
[-] 2026-03-05 09:22 PST Runtime validation pending: confirm `[AUDIO][UPSAMPLE]` and `[AUDIO][TXQ]` logs while checking speech clarity/pitch distortion.
