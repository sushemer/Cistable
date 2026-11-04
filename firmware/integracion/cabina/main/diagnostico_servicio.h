#pragma once
#include "diagnostico.h"
#include "esp_err.h"
esp_err_t ci_diag_service_start(uint32_t period_ms);
void ci_diag_service_tick(ci_diagnostics *d,const cc_config *c,const cc_state *s,cc_output o,uint64_t now);
