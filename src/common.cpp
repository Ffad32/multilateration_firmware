#include "common.h"

// Connection pins
const int8_t  PIN_RST = -1; // -1 = not connected / unused
const uint8_t PIN_IRQ = 4;
const uint8_t PIN_SS  = 5;

/* Default communication configuration. We use default non-STS DW mode. */
dwt_config_t config = {
    5,               /* Channel number. */
    DWT_PLEN_128,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* SFD type: 0=standard 8-symbol, 1=non-standard 8-symbol,
                      *           2=non-standard 16-symbol, 3=4z 8-symbol. */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    DWT_PHRRATE_STD, /* PHY header rate. */
    (129 + 8 - 8),   /* SFD timeout (preamble length + 1 + SFD length - PAC size). RX only. */
    DWT_STS_MODE_OFF, /* STS disabled. */
    DWT_STS_LEN_64,  /* STS length (see dwt_sts_lengths_e). */
    DWT_PDOA_M0      /* PDOA mode off. */
};

/* Frame sequence number, incremented after each transmission. */
uint8_t frame_seq_nb = 0;

/* Hold copy of status register state here for reference so that it can be
 * examined at a debug breakpoint. */
uint32_t status_reg = 0;
