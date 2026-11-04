#ifndef CI_DEMO_H
#define CI_DEMO_H
#include "control_cabina.h"
cc_config ci_demo_config(void);
ct_frame ci_demo_frame(uint64_t time_us);
cc_conditions ci_demo_conditions(uint64_t time_us);
ct_frame ci_demo_visual_frame(uint64_t time_us);
cc_conditions ci_demo_visual_conditions(uint64_t time_us);
#endif
