/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "server.h"

const static char *TAG = "adc";


#if (SOC_ADC_PERIPH_NUM >= 2) && !CONFIG_IDF_TARGET_ESP32C3
/**
 * On ESP32C3, ADC2 is no longer supported, due to its HW limitation.
 * Search for errata on espressif website for more details.
 */
#define EXAMPLE_USE_ADC2            1
#endif


#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

void adc_iot_init(server_ctx_t *server)
{
    //-------------ADC1 Init---------------//
    server->init_config1 = (adc_oneshot_unit_init_cfg_t) {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&server->init_config1, &server->adc1_handle));

    //-------------ADC1 Config---------------//
    server->config =  (adc_oneshot_chan_cfg_t) {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(server->adc1_handle, ADC_CHANNEL_6, &server->config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(server->adc1_handle, ADC_CHANNEL_7, &server->config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(server->adc1_handle, ADC_CHANNEL_4, &server->config));

    //-------------ADC1 Calibration Init---------------//
    server->adc1_cali_chan6_handle = NULL;
    server->adc1_cali_chan7_handle = NULL;
    server->adc1_cali_chan4_handle = NULL;
    server->do_calibration1_chan6 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, EXAMPLE_ADC_ATTEN, &server->adc1_cali_chan6_handle);
    server->do_calibration1_chan7 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_7, EXAMPLE_ADC_ATTEN, &server->adc1_cali_chan7_handle);
    server->do_calibration1_chan4 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_4, EXAMPLE_ADC_ATTEN, &server->adc1_cali_chan4_handle);


//    while (1) {
//        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw[0][0]));
//        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_6, adc_raw[0][0]);
//        if (do_calibration1_chan0) {
//            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0][0], &voltage[0][0]));
//            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_6, voltage[0][0]);
//        }
//        vTaskDelay(pdMS_TO_TICKS(1000));
//
//        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &adc_raw[0][1]));
//        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_7, adc_raw[0][1]);
//        if (do_calibration1_chan1) {
//            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan1_handle, adc_raw[0][1], &voltage[0][1]));
//            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_7, voltage[0][1]);
//        }
//        vTaskDelay(pdMS_TO_TICKS(1000));
//
//    }


}
void adc_iot_read(server_ctx_t *server) {
	int raw;
        ESP_ERROR_CHECK(adc_oneshot_read(server->adc1_handle, ADC_CHANNEL_6, &raw));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_6, &raw);
        if (server->do_calibration1_chan6) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(server->adc1_cali_chan6_handle, raw, &server->voltage6));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_6, server->voltage6);
        }


        ESP_ERROR_CHECK(adc_oneshot_read(server->adc1_handle, ADC_CHANNEL_7, &raw));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_7, &raw);
        if (server->do_calibration1_chan7) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(server->adc1_cali_chan7_handle, raw, &server->voltage7));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_7, server->voltage7);
        }


        ESP_ERROR_CHECK(adc_oneshot_read(server->adc1_handle, ADC_CHANNEL_4, &raw));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_4, &raw);
        if (server->do_calibration1_chan4) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(server->adc1_cali_chan4_handle, raw, &server->voltage4));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_4, server->voltage4);
        }

}
void adc_iot_deinit(server_ctx_t *server){
    	//Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(server->adc1_handle));
    if (server->do_calibration1_chan6) {
        example_adc_calibration_deinit(server->adc1_cali_chan6_handle);
    }
    if (server->do_calibration1_chan7) {
        example_adc_calibration_deinit(server->adc1_cali_chan7_handle);
    }
    if (server->do_calibration1_chan4) {
        example_adc_calibration_deinit(server->adc1_cali_chan4_handle);
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
