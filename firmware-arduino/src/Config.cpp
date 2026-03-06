#include <nvs_flash.h>
#include "Config.h"

// ! define preferences
Preferences preferences;
OtaStatus otaState = OTA_IDLE;
bool factory_reset_status = false;
volatile bool sleepRequested = false;

/**
 * Configuration for Elato Firmware
 * 
 * DEVELOPMENT vs PRODUCTION vs ELATO SETUP:
 * --------------------------------
 * 1. Define `DEV_MODE` in your config.h file to use local development servers
 * 2. `DEV_MODE` requires updating the IP addresses to your local network IP
 * 3. Without `DEV_MODE` defined, the firmware will use your production servers
 *
 * DEV SETUP (find your local IP address using ifconfig):
 *   - WebSocket: Your local IP (e.g., 192.168.1.100:8000)
 *   - Backend: Your local IP (e.g., 192.168.1.100:3000)
 *   - No SSL certificates required
 *
 * PROD SETUP:
 *   - WebSocket: <your-websocket-server>.deno.dev (port 443)
 *   - Backend: <your-vercel-backend-server> (port 3000)
 *   - Use your own SSL certificates (set in Config.cpp)
 * 
 * ELATO SETUP:
 *   - WebSocket: talkedge.deno.dev (port 443)
 *   - Backend: https://www.elatoai.com (port 3000)
 *   - Uses pre-configured SSL certificates (set in Config.cpp)
 */

#ifdef DEV_MODE
const char *ws_server = "172.20.10.2";
const uint16_t ws_port = 8000;
const char *ws_path = "/";
// Backend server details 
const char *backend_server = "172.20.10.2";
const uint16_t backend_port = 3000;

#elif defined(PROD_MODE)
// PROD
const char *ws_server = "<your-edge-server>.deno.dev";
const uint16_t ws_port = 443;
const char *ws_path = "/";
// Backend server details 
const char *backend_server = "<your-backend-server-url>"; // like www.facebook.com or facebook.vercel.app
const uint16_t backend_port = 3000;

#elif defined(ELATO_MODE)
// ELATO
const char *ws_server = "talkedge.deno.dev";
const uint16_t ws_port = 443;
const char *ws_path = "/";
// Backend server details 
const char *backend_server = "www.elatoai.com"; // like www.facebook.com or facebook.vercel.app
const uint16_t backend_port = 3000;
#endif

String authTokenGlobal;
volatile DeviceState deviceState = IDLE;

// I2S and Audio parameters
#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker official record/play baseline uses 16k sample rate on the host path.
// Keep this profile locked to 16k for predictable XMOS framing and voice pitch.
const uint32_t SAMPLE_RATE = 16000;
#else
const uint32_t SAMPLE_RATE = 24000;
#endif
const uint32_t MIC_SAMPLE_RATE = 16000;
// Digital gain applied to microphone samples before sending to the backend.
const float MIC_GAIN = 5.0f;

#if defined(ELATO_BOARD_RESPEAKER_LITE) && defined(ELATO_BOARD_XIAO_ESP32S3_SENSE)
#error "Board profile conflict: define only one of ELATO_BOARD_RESPEAKER_LITE or ELATO_BOARD_XIAO_ESP32S3_SENSE."
#endif

// ----------------- Pin Definitions -----------------
#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker Lite uses shared I2S clocks and separate DIN/DOUT data lines.
const i2s_port_t I2S_PORT_IN = I2S_NUM_1;
const i2s_port_t I2S_PORT_OUT = I2S_NUM_0;
#elif defined(ELATO_BOARD_XIAO_ESP32S3_SENSE)
// PDM RX is only supported on I2S0, so swap ports on XIAO ESP32S3 Sense.
const i2s_port_t I2S_PORT_IN = I2S_NUM_0;
const i2s_port_t I2S_PORT_OUT = I2S_NUM_1;
#else
const i2s_port_t I2S_PORT_IN = I2S_NUM_1;
const i2s_port_t I2S_PORT_OUT = I2S_NUM_0;
#endif

#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker Lite status LED is WS2812 (single pixel on A0) handled in LEDHandler.
// Keep legacy 3-pin RGB placeholders disabled for this board profile.
const int RED_LED_PIN = -1;
const int GREEN_LED_PIN = -1;
const int BLUE_LED_PIN = -1;
#else
const int RED_LED_PIN = GPIO_NUM_1;
const int GREEN_LED_PIN = GPIO_NUM_2;
const int BLUE_LED_PIN = GPIO_NUM_5;
#endif


#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker Lite I2S data from XMOS to ESP32 (DIN) on GPIO44.
const bool MIC_INPUT_IS_PDM = false;
const int I2S_SD = 44;
// Shared I2S clocks from host profile.
const int I2S_WS = 7;
const int I2S_SCK = 8;
#elif defined(ELATO_BOARD_XIAO_ESP32S3_SENSE)
// XIAO ESP32S3 Sense onboard PDM mic pins.
const bool MIC_INPUT_IS_PDM = true;
const int I2S_SD = 41;  // PDM data
const int I2S_WS = -1;  // Not used for PDM
const int I2S_SCK = 42; // PDM clock
#else
// External I2S mic (INMP441) default pins.
const bool MIC_INPUT_IS_PDM = false;
const int I2S_SD = 14;
const int I2S_WS = 4;
const int I2S_SCK = 1;
#endif
#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker Lite host I2S pins from Seeed/ReSpeaker examples:
// BCLK=GPIO8, WS=GPIO7, DOUT=GPIO43, DIN=GPIO44.
const int I2S_WS_OUT = GPIO_NUM_7;
const int I2S_BCK_OUT = GPIO_NUM_8;
const int I2S_DATA_OUT = GPIO_NUM_43;
// No dedicated external amp shutdown pin in this profile.
const int I2S_SD_OUT = -1;
#else
// ✅ = confirmed pins for Xiao ESP32S3
const int I2S_WS_OUT = GPIO_NUM_44; //also called LRC ✅
const int I2S_BCK_OUT = GPIO_NUM_7; //✅
const int I2S_DATA_OUT = GPIO_NUM_8; // ✅
const int I2S_SD_OUT = GPIO_NUM_9; //✅
#endif

#if defined(ELATO_BOARD_RESPEAKER_LITE)
// ReSpeaker `USR` button jumper is set to D2 in this branch/workflow.
// D2 on XIAO ESP32S3 maps to GPIO3.
const gpio_num_t BUTTON_PIN = GPIO_NUM_3;
#else
const gpio_num_t BUTTON_PIN = GPIO_NUM_6; // Only RTC IO are allowed - ESP32 Pin example
#endif

// ReSpeaker Lite control bus note for future wakeword/interrupt work:
// I2C SDA=GPIO5 (D4), SCL=GPIO6 (D5), XMOS address 0x42.


// ----------------- SSL Certificates -----------------


#ifdef PROD_MODE

// add the CA cert for your backend server here `backend_server`
const char *Vercel_CA_cert = R"EOF(
-----BEGIN CERTIFICATE-----
<YOUR VERCEL CERTIFICATE HERE>
-----END CERTIFICATE-----
)EOF";

// Deno Edge Functions CA cert
// add the CA cert for your edge server here `ws_server`
const char *CA_cert = R"EOF(
-----BEGIN CERTIFICATE-----
<YOUR TALKEDGE CERTIFICATE HERE>
-----END CERTIFICATE-----
)EOF";


#elif defined(ELATO_MODE)

const char *Vercel_CA_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";


// talkedge.deno.dev CA cert
const char *CA_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

#endif
