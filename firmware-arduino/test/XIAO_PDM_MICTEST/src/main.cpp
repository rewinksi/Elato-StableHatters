#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// Pins mirror src/Config.cpp for XIAO ESP32S3 Sense.
static const bool SWAP_PDM_PINS = false;
static const int PDM_DATA_PIN = 41;
static const int PDM_CLK_PIN = 42;
static const int I2S_SD_IN = SWAP_PDM_PINS ? PDM_CLK_PIN : PDM_DATA_PIN;   // PDM data
static const int I2S_SCK_IN = SWAP_PDM_PINS ? PDM_DATA_PIN : PDM_CLK_PIN;  // PDM clock
static const int I2S_WS_OUT = 44;
static const int I2S_BCK_OUT = 7;
static const int I2S_DATA_OUT = 8;
static const int I2S_SD_OUT = 9;

// External RGB LED pins (wired by user).
static const int RGB_LED_RED = 1;
static const int RGB_LED_GREEN = 2;
static const int RGB_LED_BLUE = 5;

void forceRgbOff() {
    digitalWrite(RGB_LED_RED, LOW);
    digitalWrite(RGB_LED_GREEN, LOW);
    digitalWrite(RGB_LED_BLUE, LOW);
}

static const i2s_port_t I2S_PORT_IN = I2S_NUM_0;  // PDM RX only on I2S0
static const i2s_port_t I2S_PORT_OUT = I2S_NUM_1;

static const uint32_t MIC_SAMPLE_RATE = 16000;
// Use the same rate for playback to avoid chipmunk/slow playback without resampling.
static const uint32_t SPK_SAMPLE_RATE = MIC_SAMPLE_RATE;
static const int BITS_PER_SAMPLE = 16;

static const int TONE_HZ = 440;
static const int TONE_DURATION_MS = 2000;
static const int BEEP_HZ = 880;
static const int BEEP_DURATION_MS = 200;

static const float PLAYBACK_GAIN = 3.0f;

static const int RECORD_SECONDS = 4;
static const int RECORD_SAMPLES = MIC_SAMPLE_RATE * RECORD_SECONDS;
static int16_t recordBuffer[RECORD_SAMPLES];
static size_t lastRecordBytes = 0;

void analyzeRecording() {
    int32_t peak = 0;
    int16_t minSample = 32767;
    int16_t maxSample = -32768;
    uint64_t sumAbs = 0;
    const int sampleCount = RECORD_SAMPLES;

    for (int i = 0; i < sampleCount; i++) {
        int16_t sample = recordBuffer[i];
        if (sample < minSample) {
            minSample = sample;
        }
        if (sample > maxSample) {
            maxSample = sample;
        }
        int32_t absVal = sample < 0 ? -sample : sample;
        if (absVal > peak) {
            peak = absVal;
        }
        sumAbs += (uint32_t)absVal;
    }

    uint32_t avgAbs = (sampleCount > 0) ? (uint32_t)(sumAbs / sampleCount) : 0;
    Serial.printf("Mic stats: peak=%ld avg_abs=%lu min=%d max=%d\n",
                  (long)peak, (unsigned long)avgAbs, (int)minSample, (int)maxSample);
    if (minSample == maxSample) {
        Serial.println("Mic warning: all samples identical (likely no valid PDM data).");
    }
    Serial.print("Mic first samples:");
    for (int i = 0; i < 8; i++) {
        Serial.printf(" %d", (int)recordBuffer[i]);
    }
    Serial.println();
}

void setupI2SInput() {
    esp_err_t err = ESP_OK;
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = MIC_SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        // Seeed's docs (Arduino I2S) use WS as the PDM clock pin.
        .bck_io_num = -1,
        .ws_io_num = I2S_SCK_IN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_IN
    };

    err = i2s_driver_install(I2S_PORT_IN, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("I2S IN install failed: %d\n", err);
        return;
    }
    err = i2s_set_pin(I2S_PORT_IN, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("I2S IN set pin failed: %d\n", err);
        return;
    }
    i2s_zero_dma_buffer(I2S_PORT_IN);
}

void setupI2SOutput() {
    esp_err_t err = ESP_OK;
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SPK_SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_OUT,
        .ws_io_num = I2S_WS_OUT,
        .data_out_num = I2S_DATA_OUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    err = i2s_driver_install(I2S_PORT_OUT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("I2S OUT install failed: %d\n", err);
        return;
    }
    err = i2s_set_pin(I2S_PORT_OUT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("I2S OUT set pin failed: %d\n", err);
        return;
    }
    i2s_zero_dma_buffer(I2S_PORT_OUT);
}

void playTone(int frequency, int durationMs) {
    const float amplitude = 0.2f;
    const int totalSamples = (SPK_SAMPLE_RATE * durationMs) / 1000;
    const int chunkSamples = 256;
    int16_t buffer[chunkSamples];

    int samplesWritten = 0;
    while (samplesWritten < totalSamples) {
        int count = min(chunkSamples, totalSamples - samplesWritten);
        for (int i = 0; i < count; i++) {
            float t = (float)(samplesWritten + i) / (float)SPK_SAMPLE_RATE;
            float sample = sinf(2.0f * PI * frequency * t);
            buffer[i] = (int16_t)(sample * amplitude * 32767.0f);
        }

        size_t bytesWritten = 0;
        esp_err_t err = i2s_write(I2S_PORT_OUT, buffer, count * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        if (err != ESP_OK) {
            Serial.printf("I2S write failed: %d\n", err);
            return;
        }
        samplesWritten += count;
    }
}

void recordAudio() {
    size_t bytesRead = 0;
    int16_t *writePtr = recordBuffer;
    size_t remainingBytes = sizeof(recordBuffer);
    lastRecordBytes = 0;

    while (remainingBytes > 0) {
        size_t toRead = min(remainingBytes, (size_t)512);
        esp_err_t err = i2s_read(I2S_PORT_IN, writePtr, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK) {
            Serial.printf("I2S read failed: %d\n", err);
            return;
        }
        remainingBytes -= bytesRead;
        writePtr += bytesRead / sizeof(int16_t);
        lastRecordBytes += bytesRead;
    }
}

void playRecording() {
    size_t bytesWritten = 0;
    // Apply a simple digital gain for audibility (clamped).
    static int16_t scaled[256];

    const int16_t *readPtr = recordBuffer;
    size_t remainingSamples = RECORD_SAMPLES;

    while (remainingSamples > 0) {
        size_t blockSamples = min(remainingSamples, (size_t)(sizeof(scaled) / sizeof(scaled[0])));
        for (size_t i = 0; i < blockSamples; i++) {
            int32_t v = (int32_t)lroundf((float)readPtr[i] * PLAYBACK_GAIN);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            scaled[i] = (int16_t)v;
        }

        i2s_write(I2S_PORT_OUT, scaled, blockSamples * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        remainingSamples -= blockSamples;
        readPtr += blockSamples;
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    // Keep external RGB LED off unless explicitly driven in this sketch.
    pinMode(RGB_LED_RED, OUTPUT);
    pinMode(RGB_LED_GREEN, OUTPUT);
    pinMode(RGB_LED_BLUE, OUTPUT);
    forceRgbOff();

    pinMode(I2S_SD_OUT, OUTPUT);
    digitalWrite(I2S_SD_OUT, HIGH);

    setupI2SOutput();
    setupI2SInput();
    forceRgbOff();

    if (RGB_LED_RED == I2S_WS_OUT || RGB_LED_RED == I2S_BCK_OUT ||
        RGB_LED_RED == I2S_DATA_OUT || RGB_LED_RED == I2S_SD_OUT) {
        Serial.println("Warning: RGB red pin overlaps I2S output; LED will light during audio.");
    }
    if (RGB_LED_GREEN == I2S_WS_OUT || RGB_LED_GREEN == I2S_BCK_OUT ||
        RGB_LED_GREEN == I2S_DATA_OUT || RGB_LED_GREEN == I2S_SD_OUT) {
        Serial.println("Warning: RGB green pin overlaps I2S output; LED will light during audio.");
    }
    if (RGB_LED_BLUE == I2S_WS_OUT || RGB_LED_BLUE == I2S_BCK_OUT ||
        RGB_LED_BLUE == I2S_DATA_OUT || RGB_LED_BLUE == I2S_SD_OUT) {
        Serial.println("Warning: RGB blue pin overlaps I2S output; LED will light during audio.");
    }

    Serial.println("XIAO PDM mic test starting...");
}

void loop() {
    forceRgbOff();
    Serial.println("Tone...");
    playTone(TONE_HZ, TONE_DURATION_MS);

    forceRgbOff();
    Serial.println("Recording...");
    recordAudio();
    Serial.printf("Recorded bytes: %u\n", (unsigned)lastRecordBytes);
    analyzeRecording();

    forceRgbOff();
    Serial.println("Beep...");
    playTone(BEEP_HZ, BEEP_DURATION_MS);

    forceRgbOff();
    Serial.println("Playback...");
    playRecording();

    delay(1000);
}
