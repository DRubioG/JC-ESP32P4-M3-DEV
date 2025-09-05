/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* eth2ap (Ethernet to Wi-Fi AP packet forwarding) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth_driver.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_private/wifi.h"
#include "ethernet_init.h"
#include "app_enthernet.h"

static void wifi_scan_init(void);


static const char *TAG = "eth2ap_example";
static esp_eth_handle_t s_eth_handle = NULL;
static QueueHandle_t flow_control_queue = NULL;
static EventGroupHandle_t Btn_event_group = NULL;
static bool s_sta_is_connected = false;
static bool s_ethernet_is_connected = false;
static uint8_t s_eth_mac[6];


static bool enthernet_connect = false;
static bool wifi_connect = false;

#define FLOW_CONTROL_QUEUE_TIMEOUT_MS (100)
#define FLOW_CONTROL_QUEUE_LENGTH (40)
#define FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS (100)

typedef struct {
    void *packet;
    uint16_t length;
} flow_control_msg_t;

typedef enum{
    ETHERNET_CONNECTED = BIT(0),
    ETHERNET_START = BIT(1),
}event_id_t;

// Forward packets from Wi-Fi to Ethernet
static esp_err_t pkt_wifi2eth(void *buffer, uint16_t len, void *eb)
{
    if (s_ethernet_is_connected) {
        if (esp_eth_transmit(s_eth_handle, buffer, len) != ESP_OK) {
            ESP_LOGE(TAG, "Ethernet send packet failed");
        }
    }
    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

// Forward packets from Ethernet to Wi-Fi
// Note that, Ethernet works faster than Wi-Fi on ESP32,
// so we need to add an extra queue to balance their speed difference.
static esp_err_t pkt_eth2wifi(esp_eth_handle_t eth_handle, uint8_t *buffer, uint32_t len, void *priv)
{
    esp_err_t ret = ESP_OK;
    flow_control_msg_t msg = {
        .packet = buffer,
        .length = len
    };
    if (xQueueSend(flow_control_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "send flow control message failed or timeout");
        free(buffer);
        ret = ESP_FAIL;
    }
    return ret;
}

// This task will fetch the packet from the queue, and then send out through Wi-Fi.
// Wi-Fi handles packets slower than Ethernet, we might add some delay between each transmitting.
static void eth2wifi_flow_control_task(void *args)
{
    flow_control_msg_t msg;
    int res = 0;
    uint32_t timeout = 0;
    while (1) {
        if (xQueueReceive(flow_control_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
            timeout = 0;
            if (s_sta_is_connected && msg.length) {
                do {
                    vTaskDelay(pdMS_TO_TICKS(timeout));
                    timeout += 2;
                    res = esp_wifi_internal_tx(WIFI_IF_AP, msg.packet, msg.length);
                } while (res && timeout < FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "WiFi send packet failed: %d", res);
                }
            }
            free(msg.packet);
        }
    }
    vTaskDelete(NULL);
}

// Event handler for Ethernet
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        s_ethernet_is_connected = true;
        xEventGroupSetBits(Btn_event_group,ETHERNET_CONNECTED);
        // esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, s_eth_mac);
        // esp_wifi_set_mac(WIFI_IF_AP, s_eth_mac);
        // ESP_ERROR_CHECK(esp_wifi_start());
        bsp_display_lock(0);
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"Enthernet initial success\n");
        // lv_label_ins_text(label,LV_LABEL_POS_LAST,"WIFI initial success\n");
        bsp_display_unlock();
        
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        s_ethernet_is_connected = false;
        // ESP_ERROR_CHECK(esp_wifi_stop());
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

// Event handler for Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    static uint8_t s_con_cnt = 0;
    switch (event_id) {
    case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "Wi-Fi AP got a station connected");
        if (!s_con_cnt) {
            s_sta_is_connected = true;
            esp_wifi_internal_reg_rxcb(WIFI_IF_AP, pkt_wifi2eth);
        }
        s_con_cnt++;
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "Wi-Fi AP got a station disconnected");
        s_con_cnt--;
        if (!s_con_cnt) {
            s_sta_is_connected = false;
            esp_wifi_internal_reg_rxcb(WIFI_IF_AP, NULL);
        }
        break;
    default:
        break;
    }
}

static void initialize_ethernet(void)
{
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));
    if (eth_port_cnt > 1) {
        ESP_LOGW(TAG, "multiple Ethernet devices detected, the first initialized is to be used!");
    }
    s_eth_handle = eth_handles[0];
    free(eth_handles);
    ESP_ERROR_CHECK(esp_eth_update_input_path(s_eth_handle, pkt_eth2wifi, NULL));
    bool eth_promiscuous = true;
    ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, &eth_promiscuous));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    EventBits_t uxBits; 
    uxBits = xEventGroupWaitBits(Btn_event_group,ETHERNET_CONNECTED,pdFALSE,pdFALSE,pdMS_TO_TICKS(5000));

    if(!uxBits)
    {
        bsp_display_lock(0);
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"Enthernet initial #ff0000 Failed#\n");
        // lv_label_ins_text(label,LV_LABEL_POS_LAST,"WIFI initial success\n");
        bsp_display_unlock();
    }

    // vTaskDelay(pdMS_TO_TICKS(CONFIG_EXAMPLE_ETH_DEINIT_AFTER_S * 1000));
    // ESP_LOGI(TAG, "stop and deinitialize Ethernet network...");
    // // Stop Ethernet driver state machine and destroy netif
    // for (int i = 0; i < eth_port_cnt; i++) {
    //     ESP_ERROR_CHECK(esp_eth_stop(eth_handles[i]));
    //     ESP_ERROR_CHECK(esp_eth_del_netif_glue(eth_netif_glues[i]));
    //     esp_netif_destroy(eth_netifs[i]);
    // }
    // esp_netif_deinit();
    // ESP_ERROR_CHECK(example_eth_deinit(eth_handles, eth_port_cnt));
    // ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));
}

static void initialize_wifi(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_EXAMPLE_WIFI_SSID,
            .ssid_len = strlen(CONFIG_EXAMPLE_WIFI_SSID),
            .password = CONFIG_EXAMPLE_WIFI_PASSWORD,
            .max_connection = CONFIG_EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = CONFIG_EXAMPLE_WIFI_CHANNEL // default: channel 1
        },
    };
    if (strlen(CONFIG_EXAMPLE_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    esp_wifi_get_mac(WIFI_IF_AP,s_eth_mac);
    esp_wifi_set_mac(WIFI_IF_AP, s_eth_mac);
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_scan_init();
}

static void wifi_scan_init(void)
{
    uint16_t ap_number;
    uint16_t ap_num=25;
    esp_err_t ret;
    ret = esp_wifi_scan_start(NULL,true);
    ESP_LOGI(TAG,"wifi scan = 0x%x",ret);
    esp_wifi_scan_get_ap_num(&ap_number);
    ESP_LOGI(TAG,"ap_number = %d",ap_number);
    // ap_num = 10;

    wifi_ap_record_t ap_info;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_record(&ap_info));
    // vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG,"ret_wiff= 0x%x",ret);
    if(ret == ESP_OK)
    {
        ESP_LOGI(TAG,"ssid = %s,riss = %d db",ap_info.ssid,ap_info.rssi);
        vTaskDelay(pdMS_TO_TICKS(1));
        bsp_display_lock(0);
        //  lv_label_ins_text(label,LV_LABEL_POS_LAST,"Enthernet initial success\n");
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"WIFI initial #00ff00 Success#\n");
        bsp_display_unlock();
    }
    else
    {
        bsp_display_lock(0);
        //  lv_label_ins_text(label,LV_LABEL_POS_LAST,"Enthernet initial success\n");
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"WIFI initial #ff0000 Failed#\n");
        bsp_display_unlock();
    }
}

static esp_err_t initialize_flow_control(void)
{
    flow_control_queue = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    if (!flow_control_queue) {
        ESP_LOGE(TAG, "create flow control queue failed");
        return ESP_FAIL;
    }
    BaseType_t ret = xTaskCreate(eth2wifi_flow_control_task, "flow_ctl", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "create flow control task failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void test_enthernet_init(void)
{
    Btn_event_group = xEventGroupCreate();
    xEventGroupClearBits(Btn_event_group,ETHERNET_CONNECTED);
    xEventGroupClearBits(Btn_event_group,ETHERNET_START);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(initialize_flow_control());
    initialize_wifi();
    initialize_ethernet();
}