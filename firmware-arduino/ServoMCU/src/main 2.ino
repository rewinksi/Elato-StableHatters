#include <Arduino.h>
#include <cmath>
#include <ESP32Servo.h>

namespace {

// Port of the behaviors in `ServoMCU/EyeMechEpsilon3.py`, but for ESP32 + CHC-049B-M4 4-axis joystick.
// - XY axis controls eye direction (LR + UD)
// - Twist Z axis controls eyelid openness (like the python "trim" input)
// - Upper lids follow gaze based on UD position (same math as python control_ud_and_lids)

constexpr uint32_t SERIAL_BAUD = 115200;

// CHC-049B-M4 joystick axes (4x potentiometers).
enum Axis : uint8_t { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2, AXIS_W = 3 };
constexpr uint8_t AXIS_COUNT = 4;
constexpr uint8_t axisPins[AXIS_COUNT] = {A0, A1, A2, A3};

// Servo naming matches EyeMechEpsilon3.py:
// LR = left/right eye pan, UD = up/down eye tilt,
// TL/BL = top/bottom lid (left), TR/BR = top/bottom lid (right)
enum ServoId : uint8_t { SERVO_LR = 0, SERVO_UD = 1, SERVO_TL = 2, SERVO_BL = 3, SERVO_TR = 4, SERVO_BR = 5 };
constexpr uint8_t SERVO_COUNT = 6;

// Adjust to your wiring.
constexpr uint8_t servoPins[SERVO_COUNT] = {D0, D1, D2, D3, D4, D5};

// Pulse width range for SG90 (tune if needed).
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2400;

// Input smoothing and timing.
constexpr uint32_t UPDATE_PERIOD_MS = 20; // ~50 Hz
constexpr uint32_t CALIBRATION_MS = 2000;
constexpr float AXIS_DEADZONE = 0.03f;
constexpr float AXIS_FILTER_ALPHA = 0.20f;

// Optional blink button (active-low). Set to `UINT8_MAX` to disable.
constexpr uint8_t BLINK_PIN = UINT8_MAX;

struct AxisCalibration {
  uint16_t minValue = 4095;
  uint16_t maxValue = 0;
  float filtered = 0.5f;
};

struct AngleLimits {
  float minAngle = 0.0f;
  float maxAngle = 180.0f;
};

Servo servos[SERVO_COUNT];
AxisCalibration axes[AXIS_COUNT];
AngleLimits limits[SERVO_COUNT];

float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

float applyDeadzoneCentered(float value01, float deadzone) {
  const float centered = value01 - 0.5f;
  const float absCentered = fabsf(centered);
  if (absCentered <= deadzone) return 0.5f;

  const float sign = centered < 0.0f ? -1.0f : 1.0f;
  const float scaled = (absCentered - deadzone) / (0.5f - deadzone);
  return 0.5f + sign * 0.5f * clamp01(scaled);
}

uint16_t readAxisRaw(Axis axis) { return static_cast<uint16_t>(analogRead(axisPins[axis])); }

float normalizeAxis(uint16_t raw, const AxisCalibration& cal) {
  if (cal.maxValue <= cal.minValue + 8) return 0.5f;
  const float value = static_cast<float>(raw - cal.minValue) / static_cast<float>(cal.maxValue - cal.minValue);
  return clamp01(value);
}

float scale01ToAngle(float value01, const AngleLimits& angleLimits, bool reverse = false) {
  float v = clamp01(value01);
  if (reverse) v = 1.0f - v;
  return angleLimits.minAngle + v * (angleLimits.maxAngle - angleLimits.minAngle);
}

void writeClampedAngle(ServoId servoId, float angle) {
  angle = clampFloat(angle, limits[servoId].minAngle, limits[servoId].maxAngle);
  servos[servoId].write(angle);
}

void calibratePose() {
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    servos[i].write(90.0f);
  }
}

void neutralPose() {
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    servos[i].write(90.0f);
  }
  // "Open" lids to their current max (matches python neutral()).
  servos[SERVO_TL].write(limits[SERVO_TL].maxAngle);
  servos[SERVO_BL].write(limits[SERVO_BL].maxAngle);
  servos[SERVO_TR].write(limits[SERVO_TR].maxAngle);
  servos[SERVO_BR].write(limits[SERVO_BR].maxAngle);
}

void blink() {
  servos[SERVO_TL].write(limits[SERVO_TL].minAngle);
  servos[SERVO_BL].write(limits[SERVO_BL].minAngle);
  servos[SERVO_TR].write(limits[SERVO_TR].minAngle);
  servos[SERVO_BR].write(limits[SERVO_BR].minAngle);
}

void updateEyelidLimitsFromTwist(float twist01) {
  // Equivalent to python's update_eyelid_limits(trim_value), but using joystick twist (0..1).
  // 0 -> most closed, 1 -> most open.
  const float t = clamp01(twist01);

  const float TL_max_closed = 130.0f;
  const float TL_max_open = 170.0f;
  const float BR_max_closed = 130.0f;
  const float BR_max_open = 170.0f;
  const float BL_max_closed = 50.0f;
  const float BL_max_open = 10.0f; // reversed range like python
  const float TR_max_closed = 50.0f;
  const float TR_max_open = 10.0f;

  limits[SERVO_TL] = {90.0f, TL_max_closed + (TL_max_open - TL_max_closed) * t};
  limits[SERVO_BR] = {90.0f, BR_max_closed + (BR_max_open - BR_max_closed) * t};
  limits[SERVO_BL] = {90.0f, BL_max_closed + (BL_max_open - BL_max_closed) * t};
  limits[SERVO_TR] = {90.0f, TR_max_closed + (TR_max_open - TR_max_closed) * t};
}

void controlUdAndLids(float udAngle) {
  // Port of python control_ud_and_lids(ud_angle).
  const float udMin = limits[SERVO_UD].minAngle;
  const float udMax = limits[SERVO_UD].maxAngle;
  const float tlMin = limits[SERVO_TL].minAngle;
  const float tlMax = limits[SERVO_TL].maxAngle;
  const float trMin = limits[SERVO_TR].minAngle;
  const float trMax = limits[SERVO_TR].maxAngle;
  const float blMin = limits[SERVO_BL].minAngle;
  const float blMax = limits[SERVO_BL].maxAngle;
  const float brMin = limits[SERVO_BR].minAngle;
  const float brMax = limits[SERVO_BR].maxAngle;

  const float udProgress = (udAngle - udMin) / (udMax - udMin); // 0..1

  const float tlTarget = tlMax - ((tlMax - tlMin) * (0.8f * (1.0f - udProgress)));
  const float trTarget = trMax + ((trMin - trMax) * (0.8f * (1.0f - udProgress)));
  const float blTarget = blMax + ((blMin - blMax) * (0.4f * (udProgress)));
  const float brTarget = brMax - ((brMax - brMin) * (0.4f * (udProgress)));

  writeClampedAngle(SERVO_UD, udAngle);
  writeClampedAngle(SERVO_TL, tlTarget);
  writeClampedAngle(SERVO_TR, trTarget);
  writeClampedAngle(SERVO_BL, blTarget);
  writeClampedAngle(SERVO_BR, brTarget);
}

bool isBlinkPressed() {
  if (BLINK_PIN == UINT8_MAX) return false;
  return digitalRead(BLINK_PIN) == LOW;
}

} // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(50);
  Serial.println();
  Serial.println("ServoMCU: EyeMechEpsilon3 port (ESP32 + 4-axis joystick)");

  analogReadResolution(12);
  if (BLINK_PIN != UINT8_MAX) pinMode(BLINK_PIN, INPUT_PULLUP);

  // Base limits ported from EyeMechEpsilon3.py (Min, Max).
  limits[SERVO_LR] = {40.0f, 140.0f};
  limits[SERVO_UD] = {40.0f, 140.0f};
  limits[SERVO_TL] = {90.0f, 170.0f};
  limits[SERVO_BL] = {90.0f, 10.0f};
  limits[SERVO_TR] = {90.0f, 10.0f};
  limits[SERVO_BR] = {90.0f, 160.0f};

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(servoPins[i], SERVO_MIN_US, SERVO_MAX_US);
  }

  calibratePose();
  delay(500);

  Serial.printf("Calibrating joystick axes for %lu ms... move all axes end-to-end.\n", static_cast<unsigned long>(CALIBRATION_MS));
  const uint32_t startMs = millis();
  while (millis() - startMs < CALIBRATION_MS) {
    for (uint8_t a = 0; a < AXIS_COUNT; a++) {
      const uint16_t raw = readAxisRaw(static_cast<Axis>(a));
      if (raw < axes[a].minValue) axes[a].minValue = raw;
      if (raw > axes[a].maxValue) axes[a].maxValue = raw;
    }
    delay(5);
  }
  for (uint8_t a = 0; a < AXIS_COUNT; a++) {
    Serial.printf("Axis %u cal: min=%u max=%u\n", a, axes[a].minValue, axes[a].maxValue);
  }

  neutralPose();
  Serial.println("Running.");
}

void loop() {
  static uint32_t lastUpdateMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastUpdateMs < UPDATE_PERIOD_MS) return;
  lastUpdateMs = nowMs;

  float axis01[AXIS_COUNT];
  for (uint8_t a = 0; a < AXIS_COUNT; a++) {
    const uint16_t raw = readAxisRaw(static_cast<Axis>(a));
    const float normalized = normalizeAxis(raw, axes[a]);
    axes[a].filtered = axes[a].filtered + AXIS_FILTER_ALPHA * (normalized - axes[a].filtered);
    axis01[a] = applyDeadzoneCentered(axes[a].filtered, AXIS_DEADZONE);
  }

  // Twist Z controls eyelid openness (updates eyelid max limits).
  updateEyelidLimitsFromTwist(axis01[AXIS_Z]);

  if (isBlinkPressed()) {
    blink();
    return;
  }

  // XY controls gaze direction.
  const float lrAngle = scale01ToAngle(axis01[AXIS_X], limits[SERVO_LR], /*reverse=*/true);
  const float udAngle = scale01ToAngle(axis01[AXIS_Y], limits[SERVO_UD], /*reverse=*/false);

  writeClampedAngle(SERVO_LR, lrAngle);
  controlUdAndLids(udAngle);
}
