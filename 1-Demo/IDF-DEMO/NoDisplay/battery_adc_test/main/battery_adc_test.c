#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

const static char *TAG = "EXAMPLE";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/


#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_4
#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

#define V_C_MAX                     (2500)
#define V_C_MIN                     (2250)

static int adc_raw;
static int adc_raw_;
static int voltage_;
static int voltage;
static int voltage_per;
static int voltage_per_;
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

void app_main(void)
{
     adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    //-------------ADC2 Init---------------//
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    //-------------ADC2 Calibration Init---------------//
    adc_cali_handle_t adc2_cali_handle = NULL;
    bool do_calibration2 = example_adc_calibration_init(ADC_UNIT_2, EXAMPLE_ADC2_CHAN0, EXAMPLE_ADC_ATTEN, &adc2_cali_handle);

    //-------------ADC2 Config---------------//
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_ADC2_CHAN0, &config));

    while (1) {
        for(int i=0;i<500;i++)
        {
            ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, EXAMPLE_ADC2_CHAN0,&adc_raw));
            adc_raw_ += adc_raw;
        }
        adc_raw_ = adc_raw_ / 500;
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, adc_raw_);

        if (do_calibration2) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, adc_raw_, &voltage));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, voltage);
        }

        voltage_ = voltage - V_C_MIN;
        if(voltage_ < 0)
            voltage_ = 0;
        voltage_per_ = voltage_per;
        voltage_per = voltage_ * 10000 / (V_C_MAX - V_C_MIN) / 100 ;

        voltage_per = (voltage_per_ + voltage_per) / 2;
        if(voltage_per > 100)
            voltage_per = 100;
            
        ESP_LOGI(TAG,"Battery charge: %d %%",voltage_per);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }


    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
    if (do_calibration2) {
        example_adc_calibration_deinit(adc2_cali_handle);
    }

}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}