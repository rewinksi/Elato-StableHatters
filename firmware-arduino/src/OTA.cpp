#include "OTA.h"
#include "HttpsOTAUpdate.h"
#include "esp_ota_ops.h"
#include <cstring>

HttpsOTAStatus_t otastatus;

namespace {
bool isUnset(const char *value) {
    return value == nullptr || value[0] == '\0';
}

bool isHttpUrl(const char *url) {
    if (url == nullptr) return false;
    return strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0;
}

bool isHttpsUrl(const char *url) {
    return url != nullptr && strncmp(url, "https://", 8) == 0;
}
}  // namespace

#ifndef ELATO_OTA_FIRMWARE_URL
#define ELATO_OTA_FIRMWARE_URL ""
#endif

// Option 1 OTA path: native pull OTA via HttpsOTA.
// Configure URL at build time with:
//   -D ELATO_OTA_FIRMWARE_URL=\"https://host/path/firmware.bin\"
const char *ota_firmware_url = ELATO_OTA_FIRMWARE_URL;

#ifdef ELATO_OTA_SERVER_CERT_PEM
// Optional PEM override for non-standard cert chains.
const char *server_certificate = ELATO_OTA_SERVER_CERT_PEM;
#else
// Default to existing backend CA root in Config.cpp.
const char *server_certificate = Vercel_CA_cert;
#endif

bool markOTAUpdateComplete() {
    HTTPClient http;
    bool acked = false;
    // Construct the JSON payload
    JsonDocument doc;
    doc["authToken"] = authTokenGlobal;
    
    String jsonString;
    serializeJson(doc, jsonString);

    // Initialize HTTPS connection with client
    #ifdef DEV_MODE
        http.begin("http://" + String(backend_server) + ":" + String(backend_port) + "/api/ota_update_handler");
    #else
        WiFiClientSecure client;
        client.setCACert(Vercel_CA_cert);  // Using the existing server certificate
        http.begin(client, "https://" + String(backend_server) + "/api/ota_update_handler");
    #endif

    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);  // Add timeout for reliability
    
    // Make the POST request
    int httpCode = http.POST(jsonString);
    
    // ... existing code ...
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("OTA status updated successfully");
            acked = true;
        } else {
            Serial.printf("OTA status update failed with code: %d\n", httpCode);
        }
    } else {
        Serial.printf("HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    // Never keep OTA_COMPLETE latched on backend callback failures.
    setOTAStatusInNVS(OTA_IDLE);
    return acked;
}

bool canStartOTAFromConfig() {
    if (!isHttpUrl(ota_firmware_url) || isUnset(ota_firmware_url)) {
        return false;
    }
    if (isHttpsUrl(ota_firmware_url) && isUnset(server_certificate)) {
        return false;
    }
    return true;
}

void getOTAStatusFromNVS()
{
    preferences.begin("ota", false);
    otaState = (OtaStatus)preferences.getUInt("status", OTA_IDLE);
    preferences.end();
}

void setOTAStatusInNVS(OtaStatus status)
{
    preferences.begin("ota", false);
    preferences.putUInt("status", status);
    preferences.end();
    otaState = status;
}

void loopOTA()
{
    otastatus = HttpsOTA.status();
    if (otastatus == HTTPS_OTA_SUCCESS)
    {
        Serial.println("Firmware written successfully. To reboot device, call API ESP.restart() or PUSH restart button on device");
        setOTAStatusInNVS(OTA_COMPLETE);
        ESP.restart();
    }
    else if (otastatus == HTTPS_OTA_FAIL)
    {
        Serial.println("Firmware Upgrade Fail");
        // Clear OTA state before reset to avoid an OTA reboot loop.
        setOTAStatusInNVS(OTA_IDLE);
        ESP.restart();
    }
}

void HttpEvent(HttpEvent_t *event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ERROR:
        // Serial.println("Http Event Error");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        // Serial.println("Http Event On Connected");
        break;
    case HTTP_EVENT_HEADER_SENT:
        // Serial.println("Http Event Header Sent");
        break;
    case HTTP_EVENT_ON_HEADER:
        // Serial.printf("Http Event On Header, key=%s, value=%s\n", event->header_key, event->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        break;
    case HTTP_EVENT_ON_FINISH:
        // Serial.println("Http Event On Finish");
        break;
    case HTTP_EVENT_DISCONNECTED:
        // Serial.println("Http Event Disconnected");
        break;
    }
}

bool performOTAUpdate()
{
    if (!isHttpUrl(ota_firmware_url) || isUnset(ota_firmware_url)) {
        Serial.println("OTA skipped: ota_firmware_url is not configured.");
        setOTAStatusInNVS(OTA_IDLE);
        return false;
    }
    if (isHttpsUrl(ota_firmware_url) && isUnset(server_certificate)) {
        Serial.println("OTA skipped: server_certificate is not configured for HTTPS OTA.");
        setOTAStatusInNVS(OTA_IDLE);
        return false;
    }

    Serial.println("Starting OTA Update...");
    HttpsOTA.onHttpEvent(HttpEvent);
    HttpsOTA.begin(ota_firmware_url, server_certificate);
    return true;
}
