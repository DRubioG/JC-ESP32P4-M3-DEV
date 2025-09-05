# 请使用arduino_esp32_v3.2版本
#此样例使用的屏幕是微雪的10.1寸屏幕，具体型号位：10.1-DSI-TOUCH-A

# lvgl V8.4.0

# lv_conf.h文件必要设置(lvgl_v8)
# 要将lvgl文件夹中的demos文件夹移动到同目录下的src文件夹中

```c
#define LV_COLOR_DEPTH 16

#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 1

#define LV_TICK_CUSTOM 1

#define LV_MEMCPY_MEMSET_STD 1

#define LV_ATTRIBUTE_FAST_MEM   IRAM_ATTR
```
