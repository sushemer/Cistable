#ifndef CI_CONFIGURACION_MODELO_H
#define CI_CONFIGURACION_MODELO_H
#include "control_cabina.h"
cc_config ci_live_model_config(void);
const char *ci_profile_name(void);
void ci_config_report(const cc_config *config,void (*emit)(const char *));
#endif
