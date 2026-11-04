#include "presentacion.h"
#include <math.h>
estado_sistema_t ci_display_state(cc_output o,bool model_a) {
    estado_sistema_t s={.modo=MODO_SIN_DATOS};
    if(!model_a) return s;
    if(o.status==MA_LTR_LIMIT&&o.mode==CC_LIMIT&&isfinite(o.ltr.signed_value)&&o.ltr.magnitude>=1) {
        s.modo=MODO_EN_MOVIMIENTO;s.riesgo=RIESGO_LIMITE_EXPERIMENTAL;return s;
    }
    bool usable=o.status==MA_VALID||o.status==MA_EXPERIMENTAL||o.status==MA_SIMULATED;
    if(!usable) return s;
    if(o.mode==CC_LEVEL&&isfinite(o.water_percent)&&o.water_percent>=0&&o.water_percent<=100) {
        s.modo=MODO_DETENIDO;s.nivel_agua_pct=(uint8_t)lround(o.water_percent);
    } else if(o.mode==CC_MOVING&&isfinite(o.ltr.signed_value)&&fabs(o.ltr.signed_value)<1&&o.risk<=2) {
        s.modo=MODO_EN_MOVIMIENTO;s.riesgo=(riesgo_t)o.risk;
    }
    return s;
}
const char *ci_model_notice(ma_status s) {
    switch(s) {
    case MA_LTR_LIMIT:return "LIMITE REGISTRADO; SIN PREDICCION ACTUAL";
    case MA_OUT_OF_DOMAIN:return "MODELO INVALIDO: AMPLITUD";
    case MA_NUMERICAL_ERROR:return "MODELO INVALIDO: NUMERICO";
    case MA_NO_SUPPORT:return "MODELO INVALIDO: SIN APOYO";
    default:return "NO VALIDADO";
    }
}
