#pragma once
#include "captura.h"
#include "field_espnow.h"
#include "control_cabina.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t cap_start(bool enabled,const char *role,unsigned rate);
void cap_metadata(const char *text);
void cap_board_report(const char *role);
const char *cap_console_name(void);
unsigned cap_console_baud(void); /* 0 for native USB: baud is not link speed */
void cap_rx(const field_espnow_rx_t *item);
void cap_tx_done(const uint8_t *data,size_t n,int status);
void cap_tx(const uint8_t *data,size_t n,int result);
/* decision: 0 rejected/waiting, 1 initial anchor, 2 committed integration,
 * 3 duplicate packet, 4 invalid wire, 5 query/state transition, 6 explicit arm. */
void cap_process(const field_espnow_rx_t *rx,const cc_state *s,cc_output o,
    uint64_t previous_source,uint8_t decision,uint8_t conditions,uint64_t now);
void cap_command(int command,const cc_state *s,uint8_t conditions,uint64_t now);
#ifdef __cplusplus
}
#endif
