/* Sensitivity scenarios from documented alternatives, NOT live configuration. */
#include "modelo_a.h"
#include <stdio.h>
#include <assert.h>
int main(void) {
    puts("scenario,base_m,volume_m3,depth_m,cg_ground_m,volume_percent,capacity_m3");
    const double gallons[]={1200,1300,1500};
    for(unsigned base=0;base<2;base++) for(unsigned k=0;k<3;k++) {
        ma_tank t=ma_model_a_tank();if(base) t.base_m=1.73-t.height_m/2;
        ma_geometry g;ma_water w;double h;
        assert(ma_geometry_make(&t,&g)==MA_VALID);
        double v=gallons[k]*3.785411784/1000;
        assert(ma_depth_at_volume(&g,v,&h)==MA_VALID&&ma_water_at_depth(&g,h,1000,&w)==MA_VALID);
        printf("%s_%.0fgalUS,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",base?"center_noted":"base_measured",
            gallons[k],t.base_m,v,h,w.cg_ground_m,w.volume_percent,g.capacity_m3);
    }
    return 0;
}
