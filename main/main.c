#include "apcfg_main.h"
#include "bridge_main.h"
#include "common.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"


static const char *TAG = "Main";
app_config_t global_cfg;
SemaphoreHandle_t sema_restart_to_bridge;


/**
 * @brief
 *
 * @param param
 */
void hci_task(const void *param)
{
    hci_init();
    uint8_t btn_long_press_cnt = 0;
    while(gpio_get_level(BTN_GPIO)==0){
        vTaskDelay(5);
        ESP_LOGI(TAG,"waiting");
    }
    while (1) {
        vTaskDelay(2);
        if (xSemaphoreTake(sema_restart_to_bridge, 1)) {
            hci_deinit();
            app_restart_to_bridge();
        }
        if (gpio_get_level(BTN_GPIO) == 0) {
            btn_long_press_cnt++;
            if (btn_long_press_cnt >= 3000 / 20) {
                hci_deinit();
                app_restart_to_webcfg();
            }
            if (btn_long_press_cnt % 15 == 0) {
                ESP_LOGI(TAG, "Btn Press %d", btn_long_press_cnt);
            }
        } else {
            btn_long_press_cnt = 0;
        }
    }
    ESP_LOGI(TAG, "System Software Restart");
}


/**
 * @brief
 *
 */
void app_main(void)
{
    sema_restart_to_bridge = xSemaphoreCreateBinary();

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    uint8_t ret = app_config_load(&global_cfg);

    xTaskCreate(hci_task, "hicTask", 1024*3, NULL, 1, NULL);

    if (ret == CFG_LOAD_SUCCESS && app_get_rst_dist() == RST_TO_BRIDGE) {
        ESP_LOGI(TAG, "Read Config Success , Bridge Start");
        ESP_LOGI(TAG, "STA NAME:%s", global_cfg.sta_name);
        ESP_LOGI(TAG, "STA PASS:%s", global_cfg.sta_pass);
        ESP_LOGI(TAG, "AT NAME:%s", global_cfg.ap_name);
        ESP_LOGI(TAG, "AT PASS:%s", global_cfg.ap_pass);
        ESP_LOGI(TAG, "AT MAC:" MACSTR, MAC2STR(global_cfg.mac));
        bridge_main();
    } else {
        ESP_LOGI(TAG, "Web Config Start");
        appcfg_main();
    }
}