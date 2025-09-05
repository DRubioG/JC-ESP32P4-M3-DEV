#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "pins_config.h"
#include "lvgl_port_v9.h"

void setup()
{
  Serial.begin(115200);
  Serial.println("ESP32P4 MIPI DSI LVGL");
  
  lvgl_sw_rotation_main();
}

void loop()
{

}
