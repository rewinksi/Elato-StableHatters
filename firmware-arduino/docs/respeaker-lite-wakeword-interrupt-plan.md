# ReSpeaker Lite Stage 2 Plan (Wakeword + Interrupt, XMOS-first)

Last updated: 2026-03-01 (PST)

## Scope

This is a design note only. No wakeword implementation is included in Stage 1.

## Primary direction

- Use XMOS-side pipeline/features first.
- Keep ESP32 as orchestrator for state transitions and websocket integration.
- Add fallback path for host-side wakeword only if XMOS signal path is insufficient.

## Target behavior

- Idle/listening loop waits for wake trigger.
- On wake trigger:
  - begin/continue voice capture to backend.
- During speaking playback:
  - barge-in interrupt can preempt playback when wake trigger or speech trigger is detected.
- Maintain deterministic state machine transitions and avoid task deadlocks.

## Candidate event sources

1. XMOS channel split firmware (`ch0-asr`, `ch1-mww`) where available.
2. XMOS configuration/status via I2C (`0x42`) for pipeline and control signals.
3. Local heuristic fallback on ESP32 audio stream if no reliable XMOS wake event is exposed.

## Firmware architecture additions (future)

- Add `WakewordManager` abstraction:
  - `begin()`
  - `poll()` or callback dispatch
  - `isWakeTriggered()`
- Add interrupt policy gate in audio playback path:
  - if interrupt event asserted during `SPEAKING`, stop output pipeline safely,
  - flush output buffers,
  - transition to `LISTENING`/capture path.
- Add debounce and cooldown windows to prevent trigger storms.

## State-machine semantics (proposed)

- `IDLE -> LISTENING`: normal ready state.
- `LISTENING -> PROCESSING`: after speech commit.
- `PROCESSING -> SPEAKING`: when response starts.
- `SPEAKING -> LISTENING`: response complete or barge-in interrupt.
- `ANY -> SLEEP`: existing sleep controls remain authoritative.

## Safety and reliability notes

- Use bounded queues and explicit flush points when interrupting playback.
- Avoid mutex inversion between websocket and wakeword callbacks.
- Maintain watchdog-safe task yield patterns (`vTaskDelay`/non-blocking polls).

## Acceptance criteria for Stage 2

- Wake trigger latency is acceptable for conversational UX.
- Interrupt during playback is reliable and repeatable.
- No audio task starvation or repeated driver reinitialization failures.
- Regression-free behavior when wakeword feature is disabled.

## References

- ReSpeaker Lite repo: https://github.com/respeaker/ReSpeaker_Lite
- XMOS DFU + firmware docs: see `/docs/respeaker-lite-docs.md`
- ESPHome micro wake word docs: https://esphome.io/components/micro_wake_word.html
- ESPHome voice_kit XMOS control example:
  https://raw.githubusercontent.com/esphome/home-assistant-voice-pe/dev/esphome/components/voice_kit/voice_kit.cpp
