#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "esp_lcd_jd9365_10_1.h"
#include "esp_lcd_touch_gt911.h"

#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lvgl_port.h"

#define TAG                                 "main"


#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#define BSP_LCD_DPI_BUFFER_NUMS             (2)

#define BSP_LCD_H_RES                       (800)
#define BSP_LCD_V_RES                       (1280)

#define BSP_I2C_NUM                         (I2C_NUM_1)
#define BSP_I2C_SDA                         (GPIO_NUM_7)
#define BSP_I2C_SCL                         (GPIO_NUM_8)

#define BSP_LCD_TOUCH_RST                   (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT                   (GPIO_NUM_NC)

#define BSP_LCD_RST                         (GPIO_NUM_NC)
#define BSP_LCD_BACKLIGHT                   (GPIO_NUM_NC)

#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_4
#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

#define V_C_MAX                     (2450)   // Charge to the maximum capacity
#define V_C_MIN                     (2050)  //Charge to the minimum capacity

#define V_MAX                       (2200)  //Maximum battery capacity
#define V_MIN                       (1800)  //Minimum battery capacity

static int adc_raw;
static int adc_raw_;
static int adc_raw_tep;
static int voltage_;
static int voltage;
static int voltage_per;
static int voltage_per_;
static int cnt = 0;
static bool charging_ = false;
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

static lv_obj_t *obj = NULL;
static lv_obj_t *label = NULL;
static lv_obj_t *label2 = NULL;
static lv_obj_t *label3 = NULL;
static lv_obj_t *label4 = NULL;

i2c_master_bus_handle_t i2c_handle = NULL; 

static void battery_test(void *arg)
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
        adc_raw_tep = adc_raw_;
        adc_raw_ = adc_raw_ / 500;

        if((adc_raw_tep - adc_raw_ > 50) && !charging_)
        {
            charging_ = true;
        }

        if(!charging_)
        {
            if(adc_raw_ - adc_raw_tep > 0)
                cnt++;
            else
                cnt = 0;

            if(cnt > 3)
                charging_ = true;
        }
        
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, adc_raw_);

        if (do_calibration2) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, adc_raw_, &voltage));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, voltage);
        }

        if(voltage > 2300 && !charging_)
        {
            charging_ = true;
        }

        if(charging_)
        {
            voltage_ = voltage - V_C_MIN;
            if(voltage_ < 0)
                voltage_ = 0;
            voltage_per_ = voltage_per;
            voltage_per = voltage_ * 10000 / (V_C_MAX - V_C_MIN) / 100 ;
            voltage_per = (voltage_per_ + voltage_per) / 2;
            if(voltage_per > 100)
                voltage_per = 100;
        }
        else
        {
            voltage_ = voltage - V_MIN;
            if(voltage_ < 0)
                voltage_ = 0;
            voltage_per_ = voltage_per;
            voltage_per = voltage_ * 10000 / (V_MAX - V_MIN) / 100 ;
            voltage_per = (voltage_per_ + voltage_per) / 2;
            if(voltage_per > 100)
                voltage_per = 100;
            if(voltage_per <= 0 )
                voltage_per = 1;
        }
       

        
            
        ESP_LOGI(TAG,"Battery charge: %d %%",voltage_per);

        lvgl_port_lock(-1);

        if(charging_)
            lv_label_set_text(label4,"Charging status : Charging");
        else
            lv_label_set_text(label4,"Charging status : Discharging");

        lv_label_set_text_fmt(label,"ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, adc_raw_);
        lv_label_set_text_fmt(label2,"ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, voltage);
        lv_label_set_text_fmt(label3,"Battery charge: %d %%",voltage_per);
        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }


    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
    if (do_calibration2) {
        example_adc_calibration_deinit(adc2_cali_handle);
    }

    vTaskDelete(NULL);
}

static esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    uint8_t data = (uint8_t)(255 * brightness_percent * 0.01);
    uint8_t chip_addr = 0x45;

    uint8_t data_addr = 0x96;
    uint8_t data_to_send[2] = {data_addr, data};

    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = 100 * 1000,
        .device_address = chip_addr,
    };

    i2c_master_dev_handle_t dev_handle = NULL;
    if (i2c_master_bus_add_device(i2c_handle, &i2c_dev_conf, &dev_handle) != ESP_OK)
    {
        return ESP_FAIL;
    }

    esp_err_t ret = i2c_master_transmit(dev_handle, data_to_send, sizeof(data_to_send), 50);
    if (ret != ESP_OK)
    {
        i2c_master_bus_rm_device(dev_handle);
        return ret;
    }

    i2c_master_bus_rm_device(dev_handle);

    return ESP_OK;
}


void app_main(void)
{

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
    };
    i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);

    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");

    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();

    esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

     ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_panel_io_handle_t io;
    esp_lcd_dbi_io_config_t dbi_config =JD9365_PANEL_IO_DBI_CONFIG();

    esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io);

    esp_lcd_panel_handle_t disp_panel = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);

     dpi_config.num_fbs = BSP_LCD_DPI_BUFFER_NUMS;

    jd9365_vendor_config_t vendor_config = {
        .flags = {
            .use_mipi_interface = 1,
        },
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = 2
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = 16,
        .rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
        .reset_gpio_num = BSP_LCD_RST,
        .vendor_config = &vendor_config,
    };
    esp_lcd_new_panel_jd9365(io, &lcd_dev_config, &disp_panel);
    esp_lcd_panel_reset(disp_panel);
    esp_lcd_panel_init(disp_panel);

     const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle);
    esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);

    lvgl_port_cfg_t lv_Port = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lv_Port);

    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = disp_panel,
        .control_handle = NULL,
        .buffer_size = BSP_LCD_H_RES *80,
        .double_buffer = false,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        // .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .buff_spiram =false,
            .sw_rotate = false,                /* Avoid tearing is not supported for SW rotation */
            .direct_mode = true,
            // .full_refresh = true,
        }
    };

    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
            .avoid_tearing = true,
        }
    };

    lv_display_t *disp =lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);
    
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };

    lvgl_port_add_touch(&touch_cfg);

    bsp_display_brightness_set(100);

    lvgl_port_lock(0);
    // lv_demo_music();
    // lv_demo_benchmark();
    // lv_demo_widgets();
    obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj,800,1280);

    label = lv_label_create(obj);
    lv_obj_center(label);
    lv_label_set_text_fmt(label,"ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, adc_raw_);

    label2 = lv_label_create(obj);
    lv_obj_align_to(label2,label,LV_ALIGN_OUT_BOTTOM_LEFT,0,5);
    lv_label_set_text_fmt(label2,"ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, voltage);

    label3 = lv_label_create(obj);
    lv_obj_align_to(label3,label2,LV_ALIGN_OUT_BOTTOM_LEFT,0,5);
    lv_label_set_text_fmt(label3,"Battery charge: %d %%",voltage_per);

    label4 = lv_label_create(obj);
    lv_obj_align_to(label4,label,LV_ALIGN_OUT_TOP_LEFT,0,-5);
    if(charging_)
        lv_label_set_text(label4,"Charging status : Charging");
    else
        lv_label_set_text(label4,"Charging status : Discharging");

    lvgl_port_unlock();

    xTaskCreatePinnedToCore(battery_test,"battery",4096,NULL,4,NULL,1);
}

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
