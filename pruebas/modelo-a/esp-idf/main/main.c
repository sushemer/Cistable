#include "modelo_a.h"
#include "esp_log.h"
void app_main(void) {
    ma_tank t=ma_model_a_tank(); ma_geometry g; ma_water w; double h;
    if (ma_geometry_make(&t,&g)!=MA_VALID ||
        ma_depth_at_volume(&g,ma_simulation_1300_us_gal_m3(),&h)!=MA_VALID ||
        ma_water_at_depth(&g,h,1000,&w)!=MA_VALID) {
        ESP_LOGE("MODELO_A","Geometria invalida"); return;
    }
    w.status=MA_SIMULATED; w.cg_status=MA_SIMULATED;
    ESP_LOGW("MODELO_A","%s: V=%.6f m3 h=%.6f m CG=%.6f m",
        ma_status_name(w.status),w.volume_m3,w.depth_m,w.cg_ground_m);
    ma_vehicle pending={0};
    ESP_LOGW("MODELO_A","LTR: %s; pendulo: %s",
        ma_status_name(ma_truck_ltr(&pending).status),
        ma_status_name(ma_pendulum_step(0,0,0).status));
}
