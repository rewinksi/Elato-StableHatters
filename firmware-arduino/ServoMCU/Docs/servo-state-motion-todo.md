# Servo State Motion Integration To-Do

## Goal
Integrate state-driven servo motion triggers for `IDLE`, `LISTENING`, `SPEAKING`, and transition events, using an external I2C servo controller to reduce ESP32 timing/load.

## 1) Architecture and Interface Contract
- [ ] Define motion ownership boundary:
  - [ ] ESP32 firmware publishes device state changes/events only.
  - [ ] ServoMCU subsystem converts state/events into motion sequences.
- [ ] Decide transport contract between main firmware and ServoMCU:
  - [ ] Option A: direct I2C commands from ESP32 to servo controller.
  - [ ] Option B: ESP32 -> ServoMCU command channel, ServoMCU -> servo controller.
- [ ] Freeze a minimal message schema:
  - [ ] `state` (`IDLE|LISTENING|SPEAKING|PROCESSING|WAITING|SETUP|OTA|SLEEP`).
  - [ ] `event` (`state_enter`, `state_exit`, `barge_in`, `response_start`, `response_end`).
  - [ ] `intensity` (0-100) optional for expressive scaling.

## 2) Hardware and Driver Bring-Up
- [ ] Confirm servo controller model and I2C address(es) (e.g. PCA9685 `0x40`).
- [ ] Verify bus voltage compatibility (3.3V logic side, external 5V servo rail).
- [ ] Add power budget checks:
  - [ ] Measure inrush/current per servo at startup.
  - [ ] Ensure shared ground across ESP32, controller, and servo PSU.
- [ ] Implement I2C scan + controller init in ServoMCU boot routine.
- [ ] Implement safe defaults on boot:
  - [ ] Set all channels to neutral pose.
  - [ ] Motion lockout until init complete.

## 3) Motion Engine Foundations
- [ ] Define per-servo calibration table:
  - [ ] `min_us`, `max_us`, `neutral_us`, `invert`, `slew_limit`.
- [ ] Implement normalized API (`-1.0..1.0`) to pulse conversion.
- [ ] Add trajectory primitives:
  - [ ] `hold`, `ramp`, `ease_in_out`, `sine`, `random_micro_jitter`.
- [ ] Add non-blocking scheduler/timeline so motions can blend or interrupt.
- [ ] Add global safety clamps:
  - [ ] Position bounds.
  - [ ] Max velocity/acceleration.
  - [ ] Emergency stop/freeze primitive.

## 4) State-to-Motion Mapping
- [ ] Create baseline motion profile per state:
  - [ ] `IDLE`: subtle breathing/micro-motions at low amplitude.
  - [ ] `LISTENING`: attentive pose + light rhythmic movement.
  - [ ] `SPEAKING`: expressive synchronized sequence pattern.
  - [ ] `PROCESSING`: brief focused hold or small pending animation.
  - [ ] `SLEEP`: park pose and power-minimized motion.
- [ ] Define transition animations:
  - [ ] `IDLE -> LISTENING`
  - [ ] `LISTENING -> SPEAKING`
  - [ ] `SPEAKING -> LISTENING`
  - [ ] `any -> SLEEP`
- [ ] Define interrupt rules:
  - [ ] On barge-in, immediately preempt current speaking sequence.
  - [ ] Prioritize safety + rapid settle over completing current motion.

## 5) Event Integration in Main Firmware
- [ ] Emit explicit state-enter events from existing state machine.
- [ ] Debounce duplicate state spam (send only on real change).
- [ ] Add simple retry/ack strategy if motion command delivery fails.
- [ ] Add compile/runtime feature flag:
  - [ ] `SERVO_MOTION_ENABLE`
  - [ ] fallback to no-op when disabled/unavailable.

## 6) Motion Sequence Authoring
- [ ] Create sequence definition format:
  - [ ] Option A: C++ structs in flash.
  - [ ] Option B: JSON-like table compiled into firmware.
- [ ] Author v1 sequence pack:
  - [ ] `idle_breathe_v1`
  - [ ] `listening_focus_v1`
  - [ ] `speaking_phrase_v1`
  - [ ] `processing_wait_v1`
- [ ] Add per-sequence tunables:
  - [ ] speed multiplier
  - [ ] amplitude multiplier
  - [ ] random seed window

## 7) Telemetry, Debugging, and Tuning Tools
- [ ] Add serial debug channel for motion events and active sequence IDs.
- [ ] Add command hooks for live tuning:
  - [ ] set servo neutral
  - [ ] run sequence by name
  - [ ] set amplitude/speed in real time
- [ ] Capture logs for state timeline vs motion timeline drift.

## 8) Validation and Test Plan
- [ ] Unit tests/smoke tests for pulse conversion + clamp logic.
- [ ] Hardware-in-loop tests:
  - [ ] boot to neutral
  - [ ] each state enters expected sequence
  - [ ] transitions execute once and settle cleanly
- [ ] Stress tests:
  - [ ] rapid state flapping (`LISTENING <-> SPEAKING`)
  - [ ] prolonged run (30-60 min)
  - [ ] I2C disconnect/reconnect behavior
- [ ] Thermal and power checks during longest speaking sequence.

## 9) Production Hardening
- [ ] Implement watchdog-safe behavior if motion task hangs.
- [ ] Persist last-known safe pose policy.
- [ ] Add startup self-check and fallback mode (`motion_disabled`) on controller failure.
- [ ] Document recovery procedure for servo/controller comm failures.

## 10) Deliverables
- [ ] `ServoMCU` motion engine module with state-trigger interface.
- [ ] Sequence pack v1 for `IDLE`, `LISTENING`, `SPEAKING`, transitions.
- [ ] Integration doc with command/state schema and wiring notes.
- [ ] Demo script: cycle through states and showcase sequence changes.
