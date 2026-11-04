#ifndef DISPLAY_H
#define DISPLAY_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
lv_display_t *display_init(void);
void display_lock(void);
void display_unlock(void);

#ifdef __cplusplus
}
#endif
#endif
