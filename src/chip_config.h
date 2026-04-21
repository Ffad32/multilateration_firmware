#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Chip role definitions
// ---------------------------------------------------------------------------

/** Role assigned to a chip. */
typedef enum {
    ROLE_RX = 0, /*!< Responder (anchor) — runs rx logic */
    ROLE_TX = 1, /*!< Initiator (tag)    — runs tx logic */
} chip_role_t;

// ---------------------------------------------------------------------------
// Per-chip configuration entry
// ---------------------------------------------------------------------------

/**
 * Maps one ESP32 chip ID to a role and a device number.
 *
 * For ROLE_TX the number identifies which tag this is (currently unused but
 * reserved for multi-tag setups).
 * For ROLE_RX the number identifies which anchor this is (1, 2, 3 …).
 */
typedef struct {
    uint64_t    chip_id;  /*!< ESP32 efuse MAC / chip ID (48-bit, upper 16 bits = 0) */
    chip_role_t role;     /*!< ROLE_TX or ROLE_RX */
    uint8_t     number;   /*!< Device number within its role group */
} chip_config_entry_t;

// ---------------------------------------------------------------------------
// Lookup result
// ---------------------------------------------------------------------------

/**
 * Result returned by chip_config_lookup().
 * If @c found is false the other fields are undefined.
 */
typedef struct {
    bool        found;
    chip_role_t role;
    uint8_t     number;
} chip_config_t;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Read the ESP32 chip ID and look it up in the built-in table.
 *
 * @return chip_config_t  Populated entry, or {.found=false} if not listed.
 */
chip_config_t chip_config_lookup();

/**
 * Print the chip ID and resolved configuration to Serial / UART.
 * Useful during bring-up to discover the chip ID of a new board.
 */
void chip_config_print(const chip_config_t &cfg);
