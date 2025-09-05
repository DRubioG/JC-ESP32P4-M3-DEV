#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "audio_player.h"

#include "file_iterator.h"
#include "iot_button.h"
#include "button_gpio.h"

#define TAG             "mp3_player"
#define MUSIC_DIR       "/sdcard/music"
#define BUTTON_IO_NUM   35
#define BUTTON_ACTIVE_LEVEL   0

file_iterator_instance_t *_file_iterator;
static audio_player_cb_t audio_idle_callback = NULL;
static QueueHandle_t event_queue;
static SemaphoreHandle_t semph_event;
int music_cnt = 0;
int cnt = 0;

static void audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    ESP_LOGI(TAG,"audio_player_callback %d",ctx->audio_event);
    if(ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN || ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_IDLE)
        xSemaphoreGive(semph_event);
        // xQueueSend(event_queue, &(ctx->audio_event), 0);
}

static void mp3_player_task(void *arg)
{
    audio_player_callback_event_t event;
    while(true)
    {
        bsp_extra_player_play_index(_file_iterator,cnt);
        cnt++;
        if(cnt > music_cnt)
            cnt = 0;
        xSemaphoreTake(semph_event, portMAX_DELAY);
    }

    bsp_extra_player_del();
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = bsp_sdcard_mount();
    if(ret == ESP_OK)
        ESP_LOGI(TAG, "SD card mount successfully");

    ESP_ERROR_CHECK(bsp_extra_codec_init());
    bsp_extra_codec_volume_set(40,NULL);
    bsp_extra_player_init();

    _file_iterator = file_iterator_new(MUSIC_DIR);
    music_cnt = file_iterator_get_count(_file_iterator);

    event_queue = xQueueCreate(1, sizeof(audio_player_callback_event_t));
    semph_event = xSemaphoreCreateBinary();

    bsp_extra_player_register_callback(audio_player_callback,NULL);

    xTaskCreatePinnedToCore(mp3_player_task,"mp3_player",4096,NULL,4,NULL,1);

}
