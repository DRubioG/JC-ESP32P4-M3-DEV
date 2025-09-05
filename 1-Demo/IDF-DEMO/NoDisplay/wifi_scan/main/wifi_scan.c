#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_check.h"


#include "esp_wifi.h"
#include "esp_wifi_remote.h"


#define TAG  "app_main"
#define SCAN_LIST_SIZE 20

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    uint16_t number = SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    ESP_LOGI(TAG,"-------------------------------------------------------");
    ESP_LOGI(TAG,"|\tSSID\t\t\t\t  RSSI\t\t|");

     for (int i = 0; (i < SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG,"|\t%-*s %d\t\t|",33,ap_info[i].ssid, ap_info[i].rssi);
     }
     ESP_LOGI(TAG,"-------------------------------------------------------");

}
