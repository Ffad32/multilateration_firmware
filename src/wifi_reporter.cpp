#include "wifi_reporter.h"

#include <WiFi.h>          /* ESP32 Arduino WiFi library                  */
#include <HTTPClient.h>    /* ESP32 Arduino HTTP client library            */
#include <ArduinoJson.h>   /* JSON serialisation (bblanchon/ArduinoJson)  */
#include "chip_config.h"   /* chip_config_lookup()                         */
#include "battery.h"       /* battery_read_voltage()                       */

// ---------------------------------------------------------------------------
// wifi_reporter_init
// ---------------------------------------------------------------------------
void wifi_reporter_init() {
    Serial.printf("[WiFi] Connecting to \"%s\" ...\r\n", WIFI_SSID);

    /* Start the WiFi station (client) mode and begin association. */
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    /* Wait until connected or the timeout expires. */
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= WIFI_CONNECT_TIMEOUT_MS) {
            /* Timed out — warn but do not halt; ranging still works offline. */
            Serial.println("[WiFi] Connection timed out. Continuing without WiFi.");
            return;
        }
        delay(500);
        Serial.print(".");
    }

    /* Successfully associated — print the assigned IP address. */
    Serial.printf("\r\n[WiFi] Connected. IP: %s\r\n",
                  WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------------------
// wifi_report
// ---------------------------------------------------------------------------
void wifi_report(const char *chip_id_str,
                 const double *distances,
                 int num_anchors,
                 float battery_v) {

    /* Do nothing if WiFi is not connected (e.g. after a timeout at boot). */
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Not connected — skipping report.");
        return;
    }

    // -----------------------------------------------------------------------
    // Build the JSON payload
    // StaticJsonDocument size: 256 bytes is enough for this small payload.
    // Increase if you add more fields.
    // -----------------------------------------------------------------------
    StaticJsonDocument<256> doc;

    /* Unique identifier for this device (48-bit MAC as hex string). */
    doc["chip_id"] = chip_id_str;
    doc["role"] = "tag";

    /* Per-anchor distances in metres.
     * distances[0] is unused (1-based indexing); we start from index 1. */
    JsonArray arr = doc.createNestedArray("anchor_distances");
    for (int i = 1; i <= num_anchors; i++) {
        /* Round to 2 decimal places to keep the payload compact. */
        arr.add(serialized(String(distances[i], 2)));
    }

    /* Battery voltage in volts. */
    doc["battery_v"] = serialized(String(battery_v, 2));

    /* Serialise the document to a String. */
    String body;
    serializeJson(doc, body);

    // -----------------------------------------------------------------------
    // Send the HTTP POST request
    // -----------------------------------------------------------------------
    HTTPClient http;

    /* Compose the full URL from the configured host, port and path. */
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_PATH;

    Serial.printf("[WiFi] POST %s  body: %s\r\n", url.c_str(), body.c_str());

    /* Begin the HTTP connection. */
    http.begin(url);

    /* Set the Content-Type header so the server knows we are sending JSON. */
    http.addHeader("Content-Type", "application/json");

    /* Send the POST request and capture the HTTP response code. */
    int httpCode = http.POST(body);

    if (httpCode > 0) {
        /* A positive code means the server responded (even if it is an error). */
        Serial.printf("[WiFi] Response: %d\r\n", httpCode);
    } else {
        /* A negative code means a transport-level error (e.g. connection refused). */
        Serial.printf("[WiFi] POST failed: %s\r\n", http.errorToString(httpCode).c_str());
    }

    /* Always free the HTTP connection resources. */
    http.end();
}

// ---------------------------------------------------------------------------
// wifi_report_anchor_battery
// ---------------------------------------------------------------------------
void wifi_report_anchor_battery() {
    /* Do nothing if WiFi is not connected (e.g. after a timeout at boot). */
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Not connected — skipping battery report.");
        return;
    }

    // -----------------------------------------------------------------------
    // Obtain chip ID, role, and anchor number
    // -----------------------------------------------------------------------
    chip_config_t cfg = chip_config_lookup();
    if (!cfg.found) {
        Serial.println("[WiFi] Chip config not found — cannot send battery report.");
        return;
    }

    /* Build the chip ID string (12 hex digits = 48-bit MAC). */
    uint64_t mac = 0;
    {
        /* Read the 6-byte efuse MAC and pack it into a uint64_t. */
        uint8_t m[6] = {0};
        esp_efuse_mac_get_default(m);
        for (int k = 0; k < 6; k++) {
            mac = (mac << 8) | m[k];
        }
    }
    char chip_id_str[13]; /* 12 hex digits + null terminator */
    snprintf(chip_id_str, sizeof(chip_id_str), "%04X%08X",
             (uint32_t)(mac >> 32),
             (uint32_t)(mac & 0xFFFFFFFFULL));

    /* Read the current battery voltage. */
    float bat_v = battery_read_voltage();

    // -----------------------------------------------------------------------
    // Build the JSON payload
    // -----------------------------------------------------------------------
    StaticJsonDocument<256> doc;

    doc["chip_id"] = chip_id_str;
    doc["role"] = "anchor";
    doc["anchor_number"] = cfg.number;
    doc["battery_v"] = serialized(String(bat_v, 2));

    /* Serialise the document to a String. */
    String body;
    serializeJson(doc, body);

    // -----------------------------------------------------------------------
    // Send the HTTP POST request
    // -----------------------------------------------------------------------
    HTTPClient http;

    /* Compose the full URL from the configured host, port and path. */
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_PATH;

    Serial.printf("[WiFi] POST %s  body: %s\r\n", url.c_str(), body.c_str());

    /* Begin the HTTP connection. */
    http.begin(url);

    /* Set the Content-Type header so the server knows we are sending JSON. */
    http.addHeader("Content-Type", "application/json");

    /* Send the POST request and capture the HTTP response code. */
    int httpCode = http.POST(body);

    if (httpCode > 0) {
        /* A positive code means the server responded (even if it is an error). */
        Serial.printf("[WiFi] Response: %d\r\n", httpCode);
    } else {
        /* A negative code means a transport-level error (e.g. connection refused). */
        Serial.printf("[WiFi] POST failed: %s\r\n", http.errorToString(httpCode).c_str());
    }

    /* Always free the HTTP connection resources. */
    http.end();
}
