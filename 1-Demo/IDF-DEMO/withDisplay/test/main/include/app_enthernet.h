#ifndef _ENTHERNET_H
#define _ENTHERNET_H

#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "c" {
#endif

extern lv_obj_t *label;

void test_enthernet_init(void);
void get_enthernet_Result(void);

#ifdef __cplusplus
}
#endif

#endif