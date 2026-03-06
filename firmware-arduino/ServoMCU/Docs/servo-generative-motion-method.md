# Generative Continuous Servo Motion Method (Non-Blocking)

## Objective
Run continuous, expressive servo motion tied to voice assistant states (`IDLE`, `LISTENING`, `SPEAKING`) without impacting real-time audio/websocket behavior.

## Key Constraint (important)
Typical I2C PWM controllers (for example PCA9685) only offload PWM signal generation. They do **not** natively execute "move from X to Y in Z seconds" trajectories.

That means:
- PWM timing is offloaded (good).
- Motion trajectory timing/interpolation still needs to be handled in firmware logic (or by a smarter external motion controller).

## Recommended First-Pass Architecture

1. State-driven motion engine (event-driven)
- Maintain a `current_state` and `active_motion_profile`.
- On state changes from firmware (`IDLE`, `LISTENING`, `SPEAKING`), switch motion profile.
- Never block waiting for a move to complete.

2. Fixed-rate motion update task
- Run a dedicated motion update tick at 20-50 Hz (e.g. every 20-40 ms).
- Each tick computes new target servo positions from profile functions.
- Write new pulse values to I2C servo controller.

3. Profile = generator function
- Define each profile as a function of time `t` + random seed + state intensity.
- Example outputs are normalized `[-1.0, 1.0]` per channel.
- Convert normalized output to pulse width with calibration and limits.

4. Safety envelope
- Per-servo limits (`min_us`, `max_us`, `neutral_us`, `max_delta_per_tick`).
- Slew-rate limiting prevents jerky motion and current spikes.
- Emergency freeze/park pose on comms or controller failure.

## Suggested State Profiles

- `IDLE`
  - Low-amplitude breathing + micro-jitter.
  - Long period (2-6 seconds), low speed.

- `LISTENING`
  - Attentive pose with gentle oscillation.
  - Slightly faster than idle, moderate amplitude.

- `SPEAKING`
  - Higher-energy layered waves/noise.
  - Phrase-like bursts + short pauses to feel conversational.

## Transition Behavior

- Use short transition blends (150-500 ms) between profiles.
- On interrupt/barge-in:
  - immediately reduce speaking amplitude,
  - blend into listening profile fast (100-250 ms),
  - avoid hard discontinuities.

## Non-Blocking Implementation Pattern

- Keep a shared motion state struct:
  - `profile_id`, `t_ms`, `seed`, `amplitude`, `speed`, `target[]`, `current[]`.
- Main loop/audio code only updates `profile_id` and optional modifiers.
- Motion task owns interpolation and I2C writes.
- Use mutex-free ownership where possible (single writer motion task).

## Why this is audio-safe

- No `delay()` in motion path.
- I2C writes are short and periodic.
- Motion task runs lower priority than websocket/audio tasks.
- Update rate capped to avoid bus spam.

## Optional Upgrade Paths

- If you want true hardware trajectory offload, use:
  - smart serial servos with onboard profile moves, or
  - dedicated motion coprocessor that accepts timed moves.
- Keep existing profile generator; swap backend driver only.

## Practical Defaults (start here)

- Motion tick: 25 Hz (`40 ms`).
- I2C clock: 400 kHz.
- Slew limit: 5-15 us per tick (tune per joint).
- Idle amplitude: 0.15-0.25.
- Listening amplitude: 0.25-0.4.
- Speaking amplitude: 0.4-0.7.

## Integration Steps

- Wire state events from main firmware into motion profile selector.
- Implement one servo channel end-to-end with telemetry.
- Add second/third channels once timing and jitter are validated.
- Tune amplitudes/slew while watching heap + stack telemetry.
