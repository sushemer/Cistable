#include "imu_bno085.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el BNO085 frontal y arranca sus reportes asíncronos.
bool imu_module_start(void);
// Obtiene la última muestra del sensor frontal.
bool imu_module_get_sample(imu_bno085_sample_t *sample);

#ifdef __cplusplus
}
#endif


