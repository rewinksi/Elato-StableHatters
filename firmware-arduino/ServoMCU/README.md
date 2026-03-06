# ServoMCU

Standalone PlatformIO project to drive an animatronic eye mechanism (6x SG90) from a CHC-049B-M4 4-axis joystick.

This is a port/refactor of `ServoMCU/EyeMechEpsilon3.py` (originally MicroPython on RP2040).

## Flash / Monitor

- Upload: `pio run -d ServoMCU -t upload`
- Serial monitor: `pio device monitor -d ServoMCU`

## Hardware config

- Joystick axes pins: `axisPins` in `ServoMCU/src/main.ino` (defaults: `A0..A3`)
- Servo output pins: `servoPins` in `ServoMCU/src/main.ino` (defaults: `D0..D5`)

## Controls

- X/Y: eye direction (`LR` pan + `UD` tilt)
- Twist Z: eyelid openness (adjusts eyelid max limits)
- Blink: optional button input (see `BLINK_PIN` in `ServoMCU/src/main.ino`, disabled by default)

Notes:
- SG90s typically require 5V power; don’t power 6 servos from the MCU’s regulator.
- Tie servo ground to MCU ground.
