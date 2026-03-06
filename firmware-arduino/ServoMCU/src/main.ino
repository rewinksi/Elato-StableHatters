#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <cmath>
#include <cstring>
#include <strings.h>

// Servo expression framework driving six SG90/FS90 servos via PCA9685 on Uno R3.
// Goals:
// - Snappy motion (velocity limited, not mushy smoothing).
// - Easy to cue expressions by category from either audio energy on an analog pin
//   or commands coming from another ESP32 via Serial.
// - Always print the live audio percentage to Serial for quick tuning.

// ----------------------------- Pins & Hardware ------------------------------
// PCA9685 channel numbers for each axis.
constexpr uint8_t SERVO_CHANNELS[] = {0, 1, 2, 3, 4, 5};
constexpr uint8_t SERVO_COUNT = 6; // LR, UD, TL, BL, TR, BR

// Analog pin that carries an envelope or microphone signal (0-5 V on Uno).
// Use A0 to avoid the I2C pins (A4/A5) used by the PCA9685.
constexpr uint8_t AUDIO_PIN = A0;

// ------------------------------- Tunables -----------------------------------
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t LOOP_PERIOD_MS = 10;     // control loop dt (~100 Hz)
constexpr float    SERVO_MAX_SPEED_DPS = 420.0f; // degrees per second per axis

// Audio -> expression thresholds.
constexpr float NOISE_FLOOR = 0.04f; // ignore tiny noise (0..1 scale)
constexpr float LOUD_FLOOR  = 0.75f; // above this, "surprised" kicks in
constexpr float TALK_FLOOR  = 0.35f; // above this, "talk" activates

// Sample count for RMS measurement. Higher = smoother, lower = faster.
constexpr uint16_t AUDIO_SAMPLES = 80;

// ----------------------------- Data Structures ------------------------------
struct Angle {
  float min;
  float max;
};

struct ServoChannel {
  uint8_t channel;
  float minAngle;
  float maxAngle;
  float current;    // current command (deg)
  float target;     // desired command (deg)
  bool  invert;     // if true, flip 0..1 mapping
};

enum ServoId : uint8_t { SERVO_LR = 0, SERVO_UD, SERVO_TL, SERVO_BL, SERVO_TR, SERVO_BR };

// Each expression stores absolute target angles for all servos.
struct Expression {
  const char *name;
  float pose[SERVO_COUNT];
};

// ------------------------------ State ---------------------------------------
ServoChannel channels[SERVO_COUNT];
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo pulse range (microseconds) for typical SG90/FS90 servos.
constexpr uint16_t SERVO_MIN_US = 500;
constexpr uint16_t SERVO_MAX_US = 2400;

uint16_t angleToPulseUs(uint8_t idx, float angleDeg);
void writeServo(uint8_t idx, float angleDeg);

// Baseline neutral pose (angles are deg). Adjust once to match your mechanics.
constexpr float NEUTRAL_POSE[SERVO_COUNT] = {
  90.0f, // LR center
  90.0f, // UD center
  160.0f, // TL upper lid open
  20.0f,  // BL lower lid open (reversed horn)
  20.0f,  // TR upper lid open (reversed horn)
  150.0f  // BR lower lid open
};

// Some ready-made expressions.
const Expression EXPRESSIONS[] = {
  {"neutral",   {90,  90, 160, 20, 20, 150}},
  {"blink",     {90,  90,  90, 90, 90,  90}},
  {"surprise",  {90,  75, 175, 10, 10, 175}},
  {"talk",      {90, 100, 150, 35, 35, 140}},
  {"sleepy",    {90, 110, 110, 80, 80, 110}},
  {"side-eye",  {70,  95, 150, 30, 30, 160}},
};

const Expression *activeExpression = &EXPRESSIONS[0];
bool audioReactive = true;

// --------------------------- Utility Functions ------------------------------
template <typename T>
T clamp(T v, T lo, T hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

// ----------------------------- Servo Control --------------------------------
void attachServos() {
  // Angle limits are based on prior tuning. Adjust per axis if linkage differs.
  const Angle ranges[SERVO_COUNT] = {
    {40, 140}, // LR
    {40, 140}, // UD
    {90, 175}, // TL
    {10,  90}, // BL (reverse horn)
    {10,  90}, // TR (reverse horn)
    {90, 175}, // BR
  };

  pwm.begin();
  pwm.setOscillatorFrequency(27000000); // datasheet nominal
  pwm.setPWMFreq(50); // 50 Hz for hobby servos

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    channels[i].channel  = SERVO_CHANNELS[i];
    channels[i].minAngle = ranges[i].min;
    channels[i].maxAngle = ranges[i].max;
    channels[i].invert   = false;
    channels[i].current  = NEUTRAL_POSE[i];
    channels[i].target   = NEUTRAL_POSE[i];
  }

  // Invert the lower lids because their horns are mounted flipped.
  channels[SERVO_BL].invert = true;
  channels[SERVO_TR].invert = true;

  // Drive all channels to neutral at startup.
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    writeServo(i, channels[i].current);
  }
}

uint16_t angleToPulseUs(uint8_t idx, float angleDeg) {
  const float normalized = clamp(
    (angleDeg - channels[idx].minAngle) / (channels[idx].maxAngle - channels[idx].minAngle),
    0.0f, 1.0f);
  const float eased = channels[idx].invert ? (1.0f - normalized) : normalized;
  return static_cast<uint16_t>(lerp(SERVO_MIN_US, SERVO_MAX_US, eased));
}

void writeServo(uint8_t idx, float angleDeg) {
  const uint16_t us = angleToPulseUs(idx, angleDeg);
  pwm.writeMicroseconds(channels[idx].channel, us);
}

void moveServos(float dtSeconds) {
  const float maxStep = SERVO_MAX_SPEED_DPS * dtSeconds;
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    float diff = channels[i].target - channels[i].current;
    if (fabsf(diff) <= maxStep) {
      channels[i].current = channels[i].target;
    } else {
      channels[i].current += (diff > 0 ? maxStep : -maxStep);
    }
    channels[i].current = clamp(channels[i].current, channels[i].minAngle, channels[i].maxAngle);
    writeServo(i, channels[i].current);
  }
}

void cueExpression(const Expression &expr, float strength = 1.0f) {
  strength = clamp(strength, 0.0f, 1.0f);
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    channels[i].target = lerp(NEUTRAL_POSE[i], expr.pose[i], strength);
  }
  activeExpression = &expr;
}

// ----------------------------- Audio Intake ---------------------------------
float readAudioLevel01() {
  // Simple AC RMS. Assume signal is roughly centered ~Vcc/2 (~2.5 V).
  uint32_t sumSq = 0;
  for (uint16_t i = 0; i < AUDIO_SAMPLES; i++) {
    int sample = analogRead(AUDIO_PIN) - 512; // 10-bit, centered
    sumSq += static_cast<uint32_t>(sample * sample);
  }

  float rms = sqrtf(static_cast<float>(sumSq) / AUDIO_SAMPLES) / 512.0f;
  return clamp(rms, 0.0f, 1.0f);
}

void updateFromAudio(uint32_t nowMs) {
  static uint32_t lastPrint = 0;
  const float level = readAudioLevel01();
  const float normalized = clamp((level - NOISE_FLOOR) / (1.0f - NOISE_FLOOR), 0.0f, 1.0f);
  const int percent = static_cast<int>(normalized * 100.0f + 0.5f);

  // Cue expressions by loudness tiers.
  if (normalized > LOUD_FLOOR) {
    cueExpression(EXPRESSIONS[2], clamp((normalized - LOUD_FLOOR) / 0.25f, 0.0f, 1.0f)); // surprise
  } else if (normalized > TALK_FLOOR) {
    cueExpression(EXPRESSIONS[3], clamp((normalized - TALK_FLOOR) / (LOUD_FLOOR - TALK_FLOOR), 0.0f, 1.0f)); // talk
  } else {
    cueExpression(EXPRESSIONS[0]);
  }

  if (nowMs - lastPrint >= 120) {
    Serial.print("Audio: ");
    Serial.print(percent);
    Serial.print("% | Expr: ");
    Serial.println(activeExpression->name);
    lastPrint = nowMs;
  }
}

// ------------------------------ Commands ------------------------------------
// Text protocol (via USB Serial or optional Serial1):
//   expr <name> [0..1]  -> sets expression with optional strength
//   audio on|off         -> toggle audio reactive mode
//   blink                -> immediate blink
//   neutral              -> go back to neutral

bool readLine(Stream &port, char *buf, size_t maxLen) {
  if (!port.available()) return false;
  size_t n = port.readBytesUntil('\n', buf, maxLen - 1);
  buf[n] = '\0';
  return n > 0;
}

void handleCommand(const char *line) {
  if (strlen(line) == 0) return;

  if (strncasecmp(line, "audio", 5) == 0) {
    audioReactive = strstr(line, "off") == nullptr;
    Serial.print("Audio reactive: ");
    Serial.println(audioReactive ? "on" : "off");
    return;
  }

  if (strncasecmp(line, "blink", 5) == 0) {
    cueExpression(EXPRESSIONS[1]);
    return;
  }

  if (strncasecmp(line, "neutral", 7) == 0) {
    cueExpression(EXPRESSIONS[0]);
    return;
  }

  if (strncasecmp(line, "expr", 4) == 0) {
    const char *name = line + 4;
    while (*name == ' ') name++;
    float strength = 1.0f;

    // Parse optional strength at the end.
    const char *space = strrchr(name, ' ');
    if (space && space[1]) {
      strength = atof(space + 1);
      strength = clamp(strength, 0.0f, 1.0f);
      // Trim trailing strength from name for comparison.
      size_t nameLen = static_cast<size_t>(space - name);
      char trimmed[16];
      if (nameLen > sizeof(trimmed) - 1) nameLen = sizeof(trimmed) - 1;
      strncpy(trimmed, name, nameLen);
      trimmed[nameLen] = '\0';
      name = trimmed;
    }

    for (const auto &expr : EXPRESSIONS) {
      if (strcasecmp(expr.name, name) == 0) {
        cueExpression(expr, strength);
        return;
      }
    }
    Serial.print("Unknown expr: ");
    Serial.println(name);
  }
}

void pollCommands() {
  static char lineBuf[64];
  if (readLine(Serial, lineBuf, sizeof(lineBuf))) {
    handleCommand(lineBuf);
  }
}

// ------------------------------- Setup/Loop ---------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin();
  delay(40);
  Serial.println();
  Serial.println("ServoMCU expression controller ready.");

  attachServos();
  cueExpression(EXPRESSIONS[0]);
}

void loop() {
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if (now - lastMs < LOOP_PERIOD_MS) return;

  const float dt = (now - lastMs) / 1000.0f;
  lastMs = now;

  pollCommands();

  if (audioReactive) {
    updateFromAudio(now);
  }

  moveServos(dt);
}
