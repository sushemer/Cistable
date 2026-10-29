#ifndef DISPLAY_H
#define DISPLAY_H

#include "lvgl.h"

lv_display_t *display_init(void);
void display_lock(void);
void display_unlock(void);

#endif
