#pragma once

#include "dw3000.h"

#define APP_NAME "SS TWR RESP v1.0"

// Connection pins
// PIN_RST uses int8_t to allow -1 (disabled/unused)
extern const int8_t  PIN_RST;
extern const uint8_t PIN_IRQ;
extern const uint8_t PIN_SS;

/* Default communication configuration. We use default non-STS DW mode. */
extern dwt_config_t config;

/* Default antenna delay values for 64 MHz PRF. See NOTE 2 in rx.h / tx.h. */
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

/* Length of the common part of the message (up to and including the function code). */
#define ALL_MSG_COMMON_LEN 10

/* Index to access fields in the ranging frames. */
#define ALL_MSG_SN_IDX          2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN         4

/* Frame sequence number, incremented after each transmission. */
extern uint8_t frame_seq_nb;

/* Hold copy of status register state here for reference so that it can be
 * examined at a debug breakpoint. */
extern uint32_t status_reg;
