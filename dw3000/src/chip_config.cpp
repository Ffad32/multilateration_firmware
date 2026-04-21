#include "chip_config.h"
#include <esp_efuse.h>   // esp_efuse_mac_get_default()

// ---------------------------------------------------------------------------
// Chip registry
// ---------------------------------------------------------------------------
// Add one entry per physical board.
//
// How to find your chip ID:
//   Flash any firmware that calls chip_config_print() (or just Serial.printf
//   with the value from chip_config_get_id() below) and read the output.
//   Then paste the printed 12-digit hex value as 0x<value> in the table.
//
// Role:   ROLE_TX = initiator/tag,  ROLE_RX = responder/anchor
// Number: For ROLE_RX this is the anchor index (1, 2, 3 …).
//         For ROLE_TX this is the tag index   (1, 2, 3 …).
// ---------------------------------------------------------------------------
static const chip_config_entry_t chip_table[] = {
    // chip_id              role      number
    { 0xF8B3B74973D4, ROLE_TX, 1 },  // <-- replace with real chip IDs
    { 0xF8B3B7495D18, ROLE_RX, 1 },  // anchor 1
    { 0xF8B3B74862EC, ROLE_RX, 2 },  // anchor 2
    { 0xF8B3B747E064, ROLE_RX, 3 },  // anchor 3
};

static constexpr size_t CHIP_TABLE_LEN =
    sizeof(chip_table) / sizeof(chip_table[0]);

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/** Read the 48-bit ESP32 MAC address and pack it into a uint64_t. */
static uint64_t chip_config_get_id() {
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    uint64_t id = 0;
    for (int i = 0; i < 6; i++) {
        id = (id << 8) | mac[i];
    }
    return id;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

chip_config_t chip_config_lookup() {
    uint64_t id = chip_config_get_id();

    for (size_t i = 0; i < CHIP_TABLE_LEN; i++) {
        if (chip_table[i].chip_id == id) {
            return { true, chip_table[i].role, chip_table[i].number };
        }
    }

    return { false, ROLE_RX, 0 };
}

void chip_config_print(const chip_config_t &cfg) {
    uint64_t id = chip_config_get_id();

    // Print chip ID as 12 hex digits (48-bit MAC)
    Serial.printf("Chip ID : %04X%08X\r\n",
                  (uint32_t)(id >> 32),
                  (uint32_t)(id & 0xFFFFFFFFULL));

    if (!cfg.found) {
        Serial.printf("Config  : NOT FOUND in chip table — halting.\r\n");
        return;
    }

    Serial.printf("Role    : %s\r\n", cfg.role == ROLE_TX ? "TX (initiator/tag)"
                                                           : "RX (responder/anchor)");
    Serial.printf("Number  : %u\r\n", cfg.number);
}
