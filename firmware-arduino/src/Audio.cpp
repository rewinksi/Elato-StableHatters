#include "OTA.h"
#include "Audio.h"
#include "PitchShift.h"
#include <math.h>
#if defined(ELATO_BOARD_RESPEAKER_LITE)
#include <Wire.h>
#endif

// WEBSOCKET
SemaphoreHandle_t wsMutex;
WebSocketsClient webSocket;

// TASK HANDLES
TaskHandle_t speakerTaskHandle = NULL;
TaskHandle_t micTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// TIMING REGISTERS
volatile bool scheduleListeningRestart = false;
unsigned long scheduledTime = 0;
unsigned long speakingStartTime = 0;

// AUDIO SETTINGS
int currentVolume = 70;
float currentPitchFactor = 1.0f;
const int CHANNELS = 1;         // Mono
const int BITS_PER_SAMPLE = 16; // 16-bit audio

#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker XMOS playback path is most stable with 2ch/32-bit framing.
const int SPEAKER_TX_CHANNELS = 2;
const int SPEAKER_TX_BITS_PER_SAMPLE = 32;
// XMOS playback clock is fixed to 48k on the ReSpeaker I2S firmware path.
const uint32_t RESPEAKER_I2S_TX_RATE = 48000;
const size_t RESPEAKER_MAX_UPSAMPLE_RATIO = 4;
#else
const int SPEAKER_TX_CHANNELS = CHANNELS;
const int SPEAKER_TX_BITS_PER_SAMPLE = BITS_PER_SAMPLE;
#endif

// AUDIO OUTPUT
class BufferPrint : public Print {
public:
  explicit BufferPrint(BufferRTOS<uint8_t>& buf) : _buffer(buf) {}

  // networkTask -> webSocket.loop() -> webSocketEvent(WStype_BIN, ...) -> opusDecoder.write() -> bufferPrint.write()
  virtual size_t write(uint8_t data) override {
    if (webSocket.isConnected() && deviceState == SPEAKING) {
        return _buffer.writeArray(&data, 1);
    }
    return 1; //let opusDecoder write, otherwise thread will stuck
  }

  // networkTask -> webSocket.loop() -> webSocketEvent(WStype_BIN, ...) -> opusDecoder.write() -> bufferPrint.write()
  virtual size_t write(const uint8_t *buffer, size_t size) override {
    if (webSocket.isConnected() && deviceState == SPEAKING) {
        return _buffer.writeArray(buffer, size);
    }
    return size; //let opusDecoder write, otherwise thread will stuck
  }

private:
  BufferRTOS<uint8_t>& _buffer;
};

BufferPrint bufferPrint(audioBuffer);
OpusAudioDecoder opusDecoder;  //access guarded by wsmutex
BufferRTOS<uint8_t> audioBuffer(AUDIO_BUFFER_SIZE, AUDIO_CHUNK_SIZE);  //producer: networkTask, consumer: audioStreamTask. Thread safe in single producer->single consumer scenario.
I2SStream i2s; //access from audioStreamTask only

// ReSpeaker can require wire-format conversion (mono16 -> stereo32) before TX.
FormatConverterStream speakerFormat(i2s);

// OLD with no pitch shift
VolumeStream volume(speakerFormat); //access from audioStreamTask only
QueueStream<uint8_t> queue(audioBuffer); //access from audioStreamTask only
StreamCopy copier(volume, queue);

// NEW for pitch shift (lossy)
PitchShiftFixedOutput pitchShift(speakerFormat);
VolumeStream volumePitch(pitchShift); //access from audioStreamTask only
StreamCopy pitchCopier(volumePitch, queue);

AudioInfo info(SAMPLE_RATE, CHANNELS, BITS_PER_SAMPLE);
#if defined(ELATO_BOARD_RESPEAKER_LITE)
AudioInfo speakerTxInfo(RESPEAKER_I2S_TX_RATE, SPEAKER_TX_CHANNELS, SPEAKER_TX_BITS_PER_SAMPLE);
#else
AudioInfo speakerTxInfo(SAMPLE_RATE, SPEAKER_TX_CHANNELS, SPEAKER_TX_BITS_PER_SAMPLE);
#endif
volatile bool i2sOutputFlushScheduled = false;

#if defined(ELATO_BOARD_RESPEAKER_LITE)
namespace {
constexpr uint8_t XMOS_ADDR = 0x42;
constexpr uint8_t RESID_CONFIG = 0xF1;
constexpr uint8_t CMD_AUDIO_PA_EN = 0x10;
constexpr uint8_t AIC3204_ADDR = 0x18;
constexpr float RESPEAKER_DIGITAL_VOLUME_SCALE = 0.75f;
constexpr uint8_t RESPEAKER_CODEC_LEVEL = 0x30;
constexpr int RESPEAKER_SDA_PIN = 5;
constexpr int RESPEAKER_SCL_PIN = 6;
bool i2cControlReady = false;
bool codecWriteStatusLogged = false;
volatile float respeakerPlaybackGain = 0.22f;

void ensureRespeakerI2cControl() {
    if (i2cControlReady) return;
    Wire.begin(RESPEAKER_SDA_PIN, RESPEAKER_SCL_PIN);
    i2cControlReady = true;
}

bool xmosWrite1Byte(uint8_t resid, uint8_t cmd, uint8_t value) {
    ensureRespeakerI2cControl();
    Wire.beginTransmission(XMOS_ADDR);
    Wire.write(resid);
    Wire.write(cmd);
    Wire.write((uint8_t)1);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool aic3204WriteReg(uint8_t reg, uint8_t value) {
    ensureRespeakerI2cControl();
    Wire.beginTransmission(AIC3204_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool setRespeakerCodecOutputLevel(uint8_t level) {
    // Official Seeed method (xiao_i2c_control_volume.ino):
    // select page 1, then set output driver gain registers.
    bool ok = true;
    ok &= aic3204WriteReg(0x00, 0x01);
    ok &= aic3204WriteReg(0x10, level);
    ok &= aic3204WriteReg(0x11, level);
    ok &= aic3204WriteReg(0x12, level);
    ok &= aic3204WriteReg(0x13, level);
    if (!codecWriteStatusLogged || !ok) {
        Serial.printf("[ReSpeaker] Codec gain write %s (level=0x%02X)\n", ok ? "ok" : "failed", level);
        codecWriteStatusLogged = true;
    }
    return ok;
}

float applyRespeakerVolumePercent(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    // Conservative analog attenuation that kept speech intelligible.
    const bool codecOk = setRespeakerCodecOutputLevel(RESPEAKER_CODEC_LEVEL);

    // Additional digital attenuation keeps the stream clean under voice bursts.
    float scale = RESPEAKER_DIGITAL_VOLUME_SCALE;
    if (!codecOk) {
        // If codec control is unavailable, keep software volume even lower.
        scale *= 0.5f;
    }
    return ((float)percent / 100.0f) * scale;
}

bool xmosReadBytes(uint8_t resid, uint8_t cmd, uint8_t *value, uint8_t readBytes) {
    if (value == nullptr || readBytes == 0 || readBytes > 254) return false;
    ensureRespeakerI2cControl();

    const uint8_t responseBytes = static_cast<uint8_t>(readBytes + 1);
    Wire.beginTransmission(XMOS_ADDR);
    Wire.write(resid);
    Wire.write(cmd);
    Wire.write(responseBytes);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    const uint8_t got = Wire.requestFrom(XMOS_ADDR, responseBytes);
    if (got != responseBytes) {
        return false;
    }

    const uint8_t status = Wire.read();
    if (status != 0x00) {
        return false;
    }

    for (uint8_t i = 0; i < readBytes; i++) {
        value[i] = Wire.read();
    }
    return true;
}

void logRespeakerXmosStatus() {
    uint8_t fw[3] = {0, 0, 0};
    uint8_t mute = 0;
    const bool fwOk = xmosReadBytes(0xF0, 0xD8, fw, sizeof(fw));   // Firmware version
    const bool muteOk = xmosReadBytes(0xF1, 0x81, &mute, 1);       // Mute status

    if (fwOk) {
        Serial.printf("[ReSpeaker] XMOS firmware version: v%u.%u.%u\n", fw[0], fw[1], fw[2]);
    } else {
        Serial.println("[ReSpeaker] XMOS firmware version read failed");
    }

    if (muteOk) {
        Serial.printf("[ReSpeaker] XMOS mute status: %s\n", mute ? "muted" : "unmuted");
    }
}

void setRespeakerAmpEnabled(bool enabled) {
    // Official ReSpeaker method: XMOS RESID 0xF1, CMD 0x10 controls AUDIO_PA_EN.
    if (!xmosWrite1Byte(RESID_CONFIG, CMD_AUDIO_PA_EN, enabled ? 1 : 0)) {
        Serial.println("[ReSpeaker] Failed to set AUDIO_PA_EN over I2C");
    }
}
} // namespace

static void copyRespeakerPcmToI2s() {
    // Stable deterministic repack:
    // Opus decoder emits mono 16-bit PCM -> upsample to XMOS TX clock -> duplicate channels
    // and left-align into 32-bit slots.
    static uint8_t monoRaw[512];
    static int16_t mono16[256];
    static int16_t upsampled16[256 * RESPEAKER_MAX_UPSAMPLE_RATIO];
    static int32_t stereo32[(256 * RESPEAKER_MAX_UPSAMPLE_RATIO) * 2];
    static uint32_t txWrites = 0;
    static uint32_t txUnderruns = 0;
    static uint32_t txSamples = 0;
    static unsigned long lastStatsMs = 0;
    static bool havePrevSample = false;
    static int16_t prevSample = 0;
    static bool hasPendingByte = false;
    static uint8_t pendingByte = 0;
    static bool ratioLogged = false;
    size_t upsampleRatio = 1;
    if (SAMPLE_RATE > 0 &&
        (RESPEAKER_I2S_TX_RATE % SAMPLE_RATE) == 0 &&
        (RESPEAKER_I2S_TX_RATE / SAMPLE_RATE) >= 1 &&
        (RESPEAKER_I2S_TX_RATE / SAMPLE_RATE) <= RESPEAKER_MAX_UPSAMPLE_RATIO) {
        upsampleRatio = (size_t)(RESPEAKER_I2S_TX_RATE / SAMPLE_RATE);
    }
    if (!ratioLogged) {
        Serial.printf("[AUDIO][UPSAMPLE] decode_rate=%u tx_rate=%u ratio=%u\n",
                      (unsigned int)SAMPLE_RATE,
                      (unsigned int)RESPEAKER_I2S_TX_RATE,
                      (unsigned int)upsampleRatio);
        ratioLogged = true;
    }

    const size_t bytesRead = queue.readBytes(monoRaw, sizeof(monoRaw));
    if (bytesRead == 0) {
        txUnderruns++;
        if (millis() - lastStatsMs > 1000) {
            Serial.printf("[AUDIO][TXQ] writes=%lu underruns=%lu samples=%lu gain=%.3f ratio=%u\n",
                          (unsigned long)txWrites,
                          (unsigned long)txUnderruns,
                          (unsigned long)txSamples,
                          respeakerPlaybackGain,
                          (unsigned int)upsampleRatio);
            txWrites = 0;
            txUnderruns = 0;
            txSamples = 0;
            lastStatsMs = millis();
        }
        return;
    }

    size_t inIndex = 0;
    size_t sampleCount = 0;
    if (hasPendingByte && bytesRead > 0) {
        mono16[sampleCount++] = (int16_t)(((uint16_t)monoRaw[0] << 8) | (uint16_t)pendingByte);
        hasPendingByte = false;
        inIndex = 1;
    }
    while ((inIndex + 1) < bytesRead && sampleCount < (sizeof(mono16) / sizeof(mono16[0]))) {
        mono16[sampleCount++] = (int16_t)(((uint16_t)monoRaw[inIndex + 1] << 8) | (uint16_t)monoRaw[inIndex]);
        inIndex += 2;
    }
    if (inIndex < bytesRead) {
        pendingByte = monoRaw[inIndex];
        hasPendingByte = true;
    }
    if (sampleCount == 0) {
        return;
    }

    size_t upsampledCount = 0;
    for (size_t i = 0; i < sampleCount; ++i) {
        const int16_t currentSample = mono16[i];
        if (!havePrevSample) {
            for (size_t k = 0; k < upsampleRatio; ++k) {
                upsampled16[upsampledCount++] = currentSample;
            }
            prevSample = currentSample;
            havePrevSample = true;
            continue;
        }
        for (size_t k = 0; k < upsampleRatio; ++k) {
            const int32_t interp =
                ((int32_t)prevSample * (int32_t)(upsampleRatio - k) +
                 (int32_t)currentSample * (int32_t)k) /
                (int32_t)upsampleRatio;
            upsampled16[upsampledCount++] = (int16_t)interp;
        }
        prevSample = currentSample;
    }

    const float gain = respeakerPlaybackGain;
    for (size_t i = 0; i < upsampledCount; ++i) {
        int32_t s = (int32_t)lroundf((float)upsampled16[i] * gain);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        const int32_t packed = s << 16;
        stereo32[i * 2] = packed;
        stereo32[i * 2 + 1] = packed;
    }

    i2s.write(reinterpret_cast<const uint8_t*>(stereo32), upsampledCount * 2 * sizeof(int32_t));
    txWrites++;
    txSamples += (uint32_t)upsampledCount;

    if (millis() - lastStatsMs > 1000) {
        Serial.printf("[AUDIO][TXQ] writes=%lu underruns=%lu samples=%lu gain=%.3f ratio=%u\n",
                      (unsigned long)txWrites,
                      (unsigned long)txUnderruns,
                      (unsigned long)txSamples,
                      respeakerPlaybackGain,
                      (unsigned int)upsampleRatio);
        txWrites = 0;
        txUnderruns = 0;
        txSamples = 0;
        lastStatsMs = millis();
    }
}
#endif

static inline bool hasSpeakerShutdownPin() {
    return I2S_SD_OUT >= 0;
}

unsigned long getSpeakingDuration() {
    if (deviceState == SPEAKING && speakingStartTime > 0) {
        return millis() - speakingStartTime;
    }
    return 0;
}

// networkTask -> webSocket.loop() -> webSocketEvent(WStype_TEXT, ...) -> transitionToSpeaking()
void transitionToSpeaking() {
    vTaskDelay(50);

    // Cancel any pending delayed-listen transition from the previous turn.
    scheduleListeningRestart = false;
    scheduledTime = 0;
    i2sInputFlushScheduled = true;
    
    deviceState = SPEAKING;
#if defined(ELATO_BOARD_RESPEAKER_LITE)
    setRespeakerAmpEnabled(true);
#endif
    if (hasSpeakerShutdownPin()) {
        digitalWrite(I2S_SD_OUT, HIGH);
    }
    speakingStartTime = millis();
    
    // webSocket.enableHeartbeat(30000, 15000, 3);
    
    Serial.println("Transitioned to speaking mode");
}

// networkTask -> transitionToListening()
// ( networkTask -> webSocket.loop() -> webSocketEvent(WStype_TEXT, ...) -> (sets scheduleListeningRestart) -> networkTask -> transitionToListening() )
void transitionToListening() {
    deviceState = PROCESSING;   
    scheduleListeningRestart = false;
    Serial.println("Transitioning to listening mode");

    i2sInputFlushScheduled = true;
    i2sOutputFlushScheduled = true;

    Serial.println("Transitioned to listening mode");

    deviceState = LISTENING;
#if defined(ELATO_BOARD_RESPEAKER_LITE)
    setRespeakerAmpEnabled(false);
#endif
    if (hasSpeakerShutdownPin()) {
        digitalWrite(I2S_SD_OUT, LOW);
    }
    // webSocket.disableHeartbeat();
}

// audioStreamTask -> copier.copy() (conditional on webSocket.isConnected())
void audioStreamTask(void *parameter) {
    Serial.println("Starting I2S stream pipeline...");
    
    if (hasSpeakerShutdownPin()) {
        pinMode(I2S_SD_OUT, OUTPUT);
    }

    OpusSettings cfg;
    cfg.sample_rate = SAMPLE_RATE;
    cfg.channels = CHANNELS;
    cfg.bits_per_sample = BITS_PER_SAMPLE;
    cfg.max_buffer_size = 6144;
    Serial.printf("[AUDIO][OPUS] decode_rate=%u ch=%d bits=%d max_buf=%d\n",
                  (unsigned int)cfg.sample_rate, cfg.channels, cfg.bits_per_sample, cfg.max_buffer_size);

    xSemaphoreTake(wsMutex, portMAX_DELAY);
    opusDecoder.setOutput(bufferPrint);
    opusDecoder.begin(cfg);
    xSemaphoreGive(wsMutex);

    audioBuffer.setReadMaxWait(0);
    
    queue.begin();

    auto config = i2s.defaultConfig(TX_MODE);
    config.bits_per_sample = speakerTxInfo.bits_per_sample;
    config.sample_rate = speakerTxInfo.sample_rate;
    config.channels = speakerTxInfo.channels;
    config.pin_bck = I2S_BCK_OUT;
    config.pin_ws = I2S_WS_OUT;
    config.pin_data = I2S_DATA_OUT;
    config.port_no = I2S_PORT_OUT;
    config.copyFrom(speakerTxInfo);  
#if defined(ELATO_BOARD_RESPEAKER_LITE)
    // Match Seeed/official ReSpeaker examples: XMOS drives BCLK/WS and host TX runs as slave.
    // Reference: https://wiki.seeedstudio.com/respeaker_record_and_play/
    config.i2s_format = I2S_STD_FORMAT;
    config.is_master = false;
#endif
    i2s.begin(config);  
    Serial.printf("[AUDIO][TX] rate=%u ch=%d bits=%d bck=%d ws=%d dout=%d master=%d\n",
                  (unsigned int)config.sample_rate, config.channels, config.bits_per_sample,
                  config.pin_bck, config.pin_ws, config.pin_data, (int)config.is_master);

    if (!speakerFormat.begin(info, speakerTxInfo)) {
        Serial.println("[AUDIO][TX] speaker format converter begin failed");
    }

    // Initialize both volume streams once
    auto vcfg = volume.defaultConfig();
    vcfg.copyFrom(info);
    vcfg.allow_boost = false;
    volume.begin(vcfg);
    
    auto vcfgPitch = volumePitch.defaultConfig();
    vcfgPitch.copyFrom(info);
    vcfgPitch.allow_boost = false;
    volumePitch.begin(vcfgPitch);

#if defined(ELATO_BOARD_RESPEAKER_LITE)
    // Keep official XMOS control path active for amp toggling and startup diagnostics.
    logRespeakerXmosStatus();
    setRespeakerCodecOutputLevel(RESPEAKER_CODEC_LEVEL);
    respeakerPlaybackGain = applyRespeakerVolumePercent(currentVolume);
    setRespeakerAmpEnabled(false);
#endif

    while (1) {
        if ( i2sOutputFlushScheduled) {
            i2sOutputFlushScheduled = false;
            i2s.flush();
            speakerFormat.flush();
            volume.flush();
            volumePitch.flush();
            queue.flush();
        }

        if (webSocket.isConnected() && deviceState == SPEAKING) {
#if defined(ELATO_BOARD_RESPEAKER_LITE)
            copyRespeakerPcmToI2s();
#else
            if (currentPitchFactor != 1.0f) {
                pitchCopier.copy();
            } else {
                copier.copy();
            }
#endif
        }
        else {
            //we should always read from audioBuffer, otherwise writing thread can stuck
            queue.read();
        }
        vTaskDelay(1); 
    }
}


class WebsocketStream : public Print {
public:
    // micTask -> micToWsCopier.copyBytes() -> wsStream.write()
    virtual size_t write(uint8_t b) override {
        if (!webSocket.isConnected() || deviceState != LISTENING) {
            return 1;
        }
        
        xSemaphoreTake(wsMutex, portMAX_DELAY);
        webSocket.sendBIN(&b, 1);
        xSemaphoreGive(wsMutex);
        return 1;
    }
    
    // micTask -> micToWsCopier.copyBytes() -> wsStream.write()
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        if (size == 0 || !webSocket.isConnected() || deviceState != LISTENING) {
            return size;
        }
        
        xSemaphoreTake(wsMutex, portMAX_DELAY);
        webSocket.sendBIN(buffer, size);
        xSemaphoreGive(wsMutex);
        return size;
    }
};

WebsocketStream wsStream; //guard with wsMutex
I2SStream i2sInput; //access from micTask only
volatile bool i2sInputFlushScheduled = false;
const int MIC_COPY_SIZE = 256;

void micTask(void *parameter) {
    // Configure and start I2S input stream.
    auto i2sConfig = i2sInput.defaultConfig(RX_MODE);
    i2sConfig.bits_per_sample = BITS_PER_SAMPLE;
    i2sConfig.sample_rate = MIC_SAMPLE_RATE;
    i2sConfig.channels = CHANNELS;
    i2sConfig.signal_type = MIC_INPUT_IS_PDM ? PDM : Digital;
    i2sConfig.i2s_format = MIC_INPUT_IS_PDM ? I2S_STD_FORMAT : I2S_LEFT_JUSTIFIED_FORMAT;
    i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    // Configure your I2S input pins appropriately here:
    // For XIAO ESP32S3 Sense (PDM), Seeed's reference uses GPIO42 as WS (clock) and GPIO41 as data.
    i2sConfig.pin_bck = MIC_INPUT_IS_PDM ? -1 : I2S_SCK;
    i2sConfig.pin_ws  = MIC_INPUT_IS_PDM ? I2S_SCK : I2S_WS;
    i2sConfig.pin_data = I2S_SD;
    i2sConfig.port_no = I2S_PORT_IN;
#if defined(ELATO_BOARD_RESPEAKER_LITE)
    // ReSpeaker shares BCLK/WS with TX; keep RX side as slave to avoid clock contention.
    i2sConfig.is_master = false;
    i2sConfig.i2s_format = I2S_STD_FORMAT;
#endif
    i2sInput.begin(i2sConfig);
    Serial.printf("[AUDIO][RX] rate=%u ch=%d bits=%d bck=%d ws=%d din=%d master=%d\n",
                  (unsigned int)i2sConfig.sample_rate, i2sConfig.channels, i2sConfig.bits_per_sample,
                  i2sConfig.pin_bck, i2sConfig.pin_ws, i2sConfig.pin_data, (int)i2sConfig.is_master);

    int16_t sampleBuffer[MIC_COPY_SIZE / sizeof(int16_t)];

    while (1) {
        if (i2sInputFlushScheduled) {
            i2sInputFlushScheduled = false;
            i2sInput.flush();
        }

        if (deviceState == LISTENING && webSocket.isConnected()) {
            size_t bytesRead = i2sInput.readBytes(reinterpret_cast<uint8_t*>(sampleBuffer), MIC_COPY_SIZE);
            if (bytesRead > 0) {
                const size_t sampleCount = bytesRead / sizeof(int16_t);
                for (size_t i = 0; i < sampleCount; i++) {
                    int32_t v = (int32_t)lroundf((float)sampleBuffer[i] * MIC_GAIN);
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                    sampleBuffer[i] = (int16_t)v;
                }
                wsStream.write(reinterpret_cast<const uint8_t*>(sampleBuffer), bytesRead);
            }
            
            // Yield more frequently
            vTaskDelay(1);
        } else {
            vTaskDelay(10);
        }
    }
}

// WEBSOCKET EVENTS
// networkTask -> webSocket.loop() -> webSocketEvent()
void webSocketEvent(WStype_t type, const uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[WSc] Disconnected!\n");
        deviceState = IDLE;
        break;
    case WStype_CONNECTED:
        Serial.printf("[WSc] Connected to url: %s\n", payload);
        deviceState = PROCESSING;
        break;
    case WStype_TEXT:
    {
        Serial.printf("[WSc] get text: %s\n", payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (char *)payload);

        if (error)
        {
            Serial.println("Error deserializing JSON");
            deviceState = IDLE;
            return;
        }

        String type = doc["type"];

        // auth messages
        if (strcmp((char*)type.c_str(), "auth") == 0) {
            currentVolume = doc["volume_control"].as<int>();
            currentPitchFactor = doc["pitch_factor"].as<float>();

            bool is_ota = doc["is_ota"].as<bool>();
            bool is_reset = doc["is_reset"].as<bool>();

            // Keep remote audio handling identical to the known-good Sense pipeline.
#if defined(ELATO_BOARD_RESPEAKER_LITE)
            const float appliedVolume = applyRespeakerVolumePercent(currentVolume);
            respeakerPlaybackGain = appliedVolume;
            currentPitchFactor = 1.0f; // disable pitch morphing on ReSpeaker to reduce artifacts
#else
            volume.setVolume(currentVolume / 100.0f);
            volumePitch.setVolume(currentVolume / 100.0f);
#endif
            
            // Only initialize pitch shift if needed
#if !defined(ELATO_BOARD_RESPEAKER_LITE)
            if (currentPitchFactor != 1.0f) {
                auto pcfg = pitchShift.defaultConfig();
                pcfg.copyFrom(info);
                pcfg.pitch_shift = currentPitchFactor;
                pcfg.buffer_size = 512;
                pitchShift.begin(pcfg);
            }
#endif

            if (is_ota) {
                if (canStartOTAFromConfig()) {
                    Serial.println("OTA update received");
                    setOTAStatusInNVS(OTA_IN_PROGRESS);
                    ESP.restart();
                } else {
                    Serial.println("OTA flag received but OTA config is incomplete; skipping reboot.");
                }
            }

            if (is_reset) {
                Serial.println("Factory reset received");
                // setFactoryResetStatusInNVS(true);
                ESP.restart();
            }
        }

        // oai messages
        if (strcmp((char*)type.c_str(), "server") == 0) {
            String msg = doc["msg"];
            Serial.println(msg);

            if (strcmp((char*)msg.c_str(), "RESPONSE.COMPLETE") == 0 || strcmp((char*)msg.c_str(), "RESPONSE.ERROR") == 0) {
                Serial.println("Received RESPONSE.COMPLETE or RESPONSE.ERROR, starting listening again");

                // Check if volume_control is included in the message
                if (doc.containsKey("volume_control")) {
                    int newVolume = doc["volume_control"].as<int>();
                    currentVolume = newVolume;
#if defined(ELATO_BOARD_RESPEAKER_LITE)
                    const float appliedVolume = applyRespeakerVolumePercent(newVolume);
                    respeakerPlaybackGain = appliedVolume;
#else
                    volume.setVolume(newVolume / 100.0f);
#endif
                }

                scheduleListeningRestart = true;
                scheduledTime = millis() + 1000; // 1 second delay
            } else if (strcmp((char*)msg.c_str(), "AUDIO.COMMITTED") == 0) {
                deviceState = PROCESSING; 
            } else if (strcmp((char*)msg.c_str(), "RESPONSE.CREATED") == 0) {
                Serial.println("Received RESPONSE.CREATED, transitioning to speaking");
                scheduleListeningRestart = false;
                scheduledTime = 0;
                transitionToSpeaking();
            } else if (strcmp((char*)msg.c_str(), "SESSION.END") == 0) {
                Serial.println("Received SESSION.END, going to sleep");
                sleepRequested = true;
            }
        }
    }
        break;
    case WStype_BIN:
    {
        if (scheduleListeningRestart || deviceState != SPEAKING) {
            Serial.println("Skipping audio data due to touch interrupt.");
            break;
        }

        // Otherwise process the audio data normally
        size_t processed = opusDecoder.write(payload, length);
        if (processed != length) {
            Serial.printf("Warning: Only processed %d/%d bytes\n", processed, length);
        }
        break;
      }
    case WStype_ERROR:
        Serial.printf("[WSc] Error: %s\n", payload);    
        break;
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_PONG:
    case WStype_PING:
    case WStype_FRAGMENT_FIN:
        break;
    }
}

// wifiTask -> WIFIMANAGER::loop() -> WIFIMANAGER::tryConnect() -> connectCb() -> websocketSetup()
void websocketSetup(const String& server_domain, int port, const String& path)
{
    const String headers = "Authorization: Bearer " + String(authTokenGlobal);

    xSemaphoreTake(wsMutex, portMAX_DELAY);

    webSocket.setExtraHeaders(headers.c_str());
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1000);
    webSocket.disableHeartbeat();

    // webSocket.enableHeartbeat(30000, 15000, 3); // 30s ping interval, 15s timeout, 3 retries

    #ifdef DEV_MODE
    webSocket.begin(server_domain.c_str(), port, path.c_str());
    #else
    webSocket.beginSslWithCA(server_domain.c_str(), port, path.c_str(), CA_cert);
    #endif

    xSemaphoreGive(wsMutex);
}

// networkTask -> webSocket.loop()
void networkTask(void *parameter) {
    while (1) {
        xSemaphoreTake(wsMutex, portMAX_DELAY);

        // Check to see if a transition to listening mode is scheduled.
        if (scheduleListeningRestart && millis() >= scheduledTime) {
            transitionToListening();
        }

        webSocket.loop();
        xSemaphoreGive(wsMutex);

        vTaskDelay(1);
    }
}
