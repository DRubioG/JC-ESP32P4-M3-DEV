#pragma once

#define EXAMPLE_LVGL_PORT_TASK_MAX_DELAY_MS 500  //range 2 to 2000
#define EXAMPLE_LVGL_PORT_TASK_MIN_DELAY_MS 5     //range 1 to 100
#define EXAMPLE_LVGL_PORT_TASK_PRIORITY     4
#define EXAMPLE_LVGL_PORT_TASK_STACK_SIZE_KB  6   //KB
#define EXAMPLE_LVGL_PORT_TASK_CORE         -1  //range -1 to 1
#define EXAMPLE_LVGL_PORT_TICK              2   //ragne 1 to 100

#define EXAMPLE_LVGL_PORT_AVOID_TEAR_ENABLE   1

#ifdef  EXAMPLE_LVGL_PORT_AVOID_TEAR_ENABLE
#define EXAMPLE_LVGL_PORT_AVOID_TEAR_MODE   3   //range 1 to 3

#define EXAMPLE_LVGL_PORT_ROTATION_DEGREE_ 90   // 0,90,180 or 270
#define EXAMPLE_LVGL_PORT_PPA_ROTATION_ENABLE 1
#endif




#define LCD_H_RES 800
#define LCD_V_RES 1280

#define LCD_RST -1
#define LCD_LED -1

#define TP_I2C_SDA 7
#define TP_I2C_SCL 8
#define TP_RST -1
#define TP_INT -1
