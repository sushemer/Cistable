#include "display_module.h"

#include "display_port.h"
#include "interface.h"

void display_module_start(void){
    lv_display_t *display = display_init();
    display_lock();
    interface_init(display);
    display_unlock();
}

void display_module_update(const gps_module_snapshot_t *gps,
                           const imu_bno085_sample_t *front_imu,
                           const TankPayload *rear, bool rear_online){
    display_lock();
    interface_update(gps, front_imu, rear, rear_online);
    display_unlock();
}
