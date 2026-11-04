#ifndef CI_PRESENTACION_H
#define CI_PRESENTACION_H
#include "control_cabina.h"
#include "estado_sistema.h"
/* Pure policy, shared by the UI task and host tests; no LVGL/driver calls. */
estado_sistema_t ci_display_state(cc_output output,bool model_a);
const char *ci_model_notice(ma_status status);
#endif
