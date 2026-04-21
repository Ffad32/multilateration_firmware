#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// WiFi credentials — edit these to match your network
// ---------------------------------------------------------------------------
#define WIFI_SSID     "FANDA-PC"       /* Name of the WiFi network to join */
#define WIFI_PASSWORD "1:s2D818"   /* Password for the WiFi network     */

// ---------------------------------------------------------------------------
// HTTP server endpoint
// Edit SERVER_HOST and SERVER_PORT to point at your REST server.
// The ESP32 will POST JSON to:  http://<SERVER_HOST>:<SERVER_PORT><SERVER_PATH>
// ---------------------------------------------------------------------------
#define SERVER_HOST "192.168.1.197"     /* IP or hostname of the REST server */
#define SERVER_PORT 8080                  /* TCP port the server listens on     */
#define SERVER_PATH "/api/status"       /* URL path for the POST endpoint     */

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
/** Maximum time (ms) to wait for WiFi association before giving up. */
#define WIFI_CONNECT_TIMEOUT_MS 10000

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Connect to WiFi.  Blocks until connected or WIFI_CONNECT_TIMEOUT_MS elapses.
 * Prints progress to Serial.
 * Call once from setup().
 */
void wifi_reporter_init();

/**
 * Send a status report to the REST server via HTTP POST.
 *
 * Builds a JSON body:
 *   {
 *     "chip_id":    "<12-digit hex MAC>",
 *     "anchor_distances": [d1, d2, d3],   // metres, one per anchor
 *     "battery_v":  3.82                  // volts
 *   }
 *
 * @param chip_id_str  Null-terminated string with the chip ID (hex).
 * @param distances    Array of per-anchor distances in metres (index 1-based,
 *                     pass the full array including unused index 0).
 * @param num_anchors  Number of valid entries starting at index 1.
 * @param battery_v    Battery voltage in volts.
 */
void wifi_report(const char *chip_id_str,
                 const double *distances,
                 int num_anchors,
                 float battery_v);

/**
 * Send a battery-only report for anchors.
 *
 * Builds a JSON body:
 *   {
 *     "chip_id":      "<12-digit hex MAC>",
 *     "role":         "anchor",
 *     "anchor_number": 1,
 *     "battery_v":    3.75
 *   }
 *
 * The chip ID, role, and anchor number are obtained from chip_config_lookup().
 * Battery voltage is read via battery_read_voltage().
 */
void wifi_report_anchor_battery();
