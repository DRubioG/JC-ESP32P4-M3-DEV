#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "app_enthernet.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "bsp/esp-bsp.h"
#include "app_camera.h"

#define EXAMPLE_RECV_BUF_SIZE (2400)

#define ECHO_TEST_TXD           (GPIO_NUM_26)
#define ECHO_TEST_RXD           (GPIO_NUM_27)

// RTS for RS485 Half-Duplex Mode manages DE/~RE
#define ECHO_TEST_RTS           (UART_PIN_NO_CHANGE)

// CTS is not used in RS485 Half-Duplex Mode
#define ECHO_TEST_CTS           (UART_PIN_NO_CHANGE)

#define BUF_SIZE                (127)
#define BAUD_RATE               (115200)

#define PACKET_READ_TICS        (100 / portTICK_PERIOD_MS)
#define ECHO_TASK_STACK_SIZE    (3072)
#define ECHO_TASK_PRIO          (10)
#define ECHO_UART_PORT          (1)

#define ECHO_READ_TOUT          (3) 

const int uart_num = ECHO_UART_PORT;

static const char err_reason[][30] = {"input param is invalid",
    "operation timeout"
   };

static const char *TAG = "MAIN";
static lv_obj_t *obj = NULL;
lv_obj_t *label = NULL;
static lv_obj_t *btn = NULL;

static bool RS485_test_bool = false;

static void i2s_echo(void *args)
{
    uint8_t *mic_data = malloc(EXAMPLE_RECV_BUF_SIZE);
    if (!mic_data) {
        ESP_LOGE(TAG, "[echo] No memory for read data buffer");
        abort();
    }
    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;
    size_t bytes_write = 0;
    ESP_LOGI(TAG, "[echo] Echo start");

    while (1) {
        // ESP_LOGI(TAG,"i2s read");
        memset(mic_data, 0, EXAMPLE_RECV_BUF_SIZE);
        /* Read sample data from mic */
        ret = bsp_extra_i2s_read(mic_data, EXAMPLE_RECV_BUF_SIZE, &bytes_read, 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[echo] i2s read failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
            abort();
        }
        /* Write sample data to earphone */
        ret = bsp_extra_i2s_write(mic_data, EXAMPLE_RECV_BUF_SIZE, &bytes_write, 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[echo] i2s write failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
            abort();
        }
        if (bytes_read != bytes_write) {
            ESP_LOGW(TAG, "[echo] %d bytes read but only %d bytes are written", bytes_read, bytes_write);
        }
    }
    vTaskDelete(NULL);
}

static void echo_send(const int port, const char* str, uint8_t length)
{
    // ESP_LOGI(TAG,"data = %s",str);
    if (uart_write_bytes(port, str, length) != length) {
        ESP_LOGE(TAG, "Send data critical failure.");
        // add your code to handle sending failure here
        abort();
    }
}


static void RS485_test_callback(lv_event_t *e)
{
    // echo_send(uart_num, "\r\n", 2);
    char prefix[] = "RS485 TEST";
    echo_send(uart_num,prefix,sizeof(prefix));
    ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 10));
}

static void echo_task(void *arg)
{
    
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // // Set UART log level
    // esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Start RS485 application test and configure UART.");

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    ESP_LOGI(TAG, "UART set pins, mode and install driver.");

    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(uart_num, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, ECHO_READ_TOUT));

    // Allocate buffers for UART
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);

    ESP_LOGI(TAG, "UART start receive loop.\r");
    // echo_send(uart_num, "Start RS485 UART test.\r\n", 24);

    while (1) {
        //Read data from UART
        int len = uart_read_bytes(uart_num, data, BUF_SIZE, PACKET_READ_TICS);

        //Write data back to UART
        if (len > 0) {
            // echo_send(uart_num, "\r\n", 2);
            // char prefix[] = "RS485 Received: [";
            // echo_send(uart_num, prefix, (sizeof(prefix) - 1));
            ESP_LOGI(TAG, "Received %u bytes:", len);
            printf("[ ");
            for (int i = 0; i < len; i++) {
                printf("0x%.2x ", data[i]);
                // echo_send(uart_num, (const char*)&data[i], 1);
                // Add a Newline character if you get a return character from paste (Paste tests multibyte receipt/buffer)
                // if (data[i] == '\r') {
                //     echo_send(uart_num, "\n", 1);
                // }
            }
            printf("] \n");
            // echo_send(uart_num, "]\r\n", 3);

            char *re = strstr((char *)data,"RS485 TEST");
            if(re == NULL)
            {
                ESP_LOGI(TAG,"NOT send data");
            }
            else
            {
                if(!RS485_test_bool)
                {
                    RS485_test_bool = true;
                    lvgl_port_lock(-1);
                    lv_label_ins_text(label,LV_LABEL_POS_LAST,"RS485 initial #00ff00 Success#\n");
                    lvgl_port_unlock();
                    // break;
                }
            }

            
        }
        // } else {
        //     // Echo a "." to show we are alive while we wait for input
        //     echo_send(uart_num, ".", 1);
        //     ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 10));
        // }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret_bsp = ESP_OK;
    
    
    // bsp_i2c_init();

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    

    bsp_display_lock(0);
    obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj,800,1280);

    btn = lv_btn_create(obj);
    lv_obj_align(btn,LV_ALIGN_TOP_MID,50,50);
    lv_obj_set_size(btn,200,100);
    lv_obj_t *label_btn = lv_label_create(btn);
    lv_obj_center(label_btn);
    lv_label_set_text(label_btn,"RS485 Test");
    lv_obj_add_event_cb(btn,RS485_test_callback,LV_EVENT_CLICKED,NULL);

    label = lv_label_create(obj);
    lv_label_set_recolor(label,true);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_28,0);
    lv_label_set_text(label,"LCD initial #00ff00 Success#\n");
    lv_label_ins_text(label,LV_LABEL_POS_LAST,"TP initial #00ff00 Success#\n");
    bsp_display_unlock();

    

    esp_err_t ret;
    ret = bsp_sdcard_mount();
    if(ret == ESP_OK)
    {
        bsp_display_lock(0);
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"SDCard initial #00ff00 Success#\n");
        bsp_display_unlock();
    }
    else
    {
        bsp_display_lock(0);
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"SDCard initial #ff0000 Failed#\n");
        bsp_display_unlock();
    }
    test_enthernet_init();

    ret_bsp = bsp_extra_codec_init();
    bsp_extra_codec_volume_set(50, NULL);
    if(ret_bsp == ESP_OK)
    {
        bsp_display_lock(0);
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"ES8311 initial #00ff00 success#\n");
        bsp_display_unlock();
        // bsp_extra_codec_volume_set(50,NULL);
    }
    else
    {
        bsp_display_lock(0);
        lv_label_ins_text(label,LV_LABEL_POS_LAST,"ES8311 initial #ff0000 failed#\n");
        bsp_display_unlock();
    }
    // bsp_audio_init();
    // bsp_extra_codec_mute_set(true);
    // bsp_extra_codec_volume_set(50,NULL);
    // 

    app_camera();

    xTaskCreate(i2s_echo, "i2s_echo", 8192, NULL, 4, NULL);
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 3, NULL);
   
}
