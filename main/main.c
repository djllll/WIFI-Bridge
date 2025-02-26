#include "apcfg_main.h"
#include "bridge_main.h"
#include "common.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"


static const char *TAG = "Main";
app_config_t global_cfg;
SemaphoreHandle_t sema_config_post_finish;
SemaphoreHandle_t sema_config_err;
SemaphoreHandle_t sema_bridge_err;
QueueHandle_t queue_runmode;


#define LED_ALL_OFF()                 \
    do {                              \
        gpio_set_level(LEDB_GPIO, 1); \
        gpio_set_level(LEDG_GPIO, 1); \
    } while (0);
#define APPLED_ON(app)  gpio_set_level(app == APP_BRIDGE ? LEDG_GPIO : LEDB_GPIO, 0)
#define APPLED_OFF(app) gpio_set_level(app == APP_BRIDGE ? LEDG_GPIO : LEDB_GPIO, 1)
#define BTN_RELESED()   (gpio_get_level(BTN_GPIO))
#define BTN_PRESSING()  (!BTN_RELESED())

/**
 * @brief
 *
 * @param param
 */
void hci_task(const void *param)
{
    uint8_t btn_long_press_cnt = 0; // 按键计数
    uint8_t app_err_blink = 0;
    uint8_t app_err_flag = 0;
    app_runmode_t running_mode;

    /* 初始化 */
    hci_init();

    /*  熄灭所有led */
    LED_ALL_OFF();

    /* 等待按键松开 */
    ESP_LOGI(TAG, "Btn Release Waiting");
    while (BTN_PRESSING()) {
        vTaskDelay(5);
    }

    /* 接收当前运行的应用模式 */
    xQueueReceive(queue_runmode, &running_mode, portMAX_DELAY);
    APPLED_ON(running_mode);

    while (1) {
        vTaskDelay(20);

        /* 配置完成 */
        if (xSemaphoreTake(sema_config_post_finish, 0)) {
            ESP_LOGI(TAG, "Get config finish semaphore!");
            for (int i = 0; i < 3; i++) {
                APPLED_OFF(running_mode);
                vTaskDelay(10);
                APPLED_ON(running_mode);
                vTaskDelay(10);
            }
        }

        /* 应用错误提示 */
        if (xSemaphoreTake(sema_bridge_err, 0) || xSemaphoreTake(sema_config_err, 0)) {
            app_err_flag = 1;
        }
        if (app_err_flag) {
            app_err_blink = !app_err_blink;
            if (app_err_blink) {
                APPLED_ON(running_mode);
            } else {
                APPLED_OFF(running_mode);
            }
        }


        /* 按键长按检测 */
        if (BTN_PRESSING()) {
            btn_long_press_cnt++;
            if (btn_long_press_cnt >= 15) {
                LED_ALL_OFF();
                if (running_mode == APP_CONFIG) {
                    APPLED_ON(APP_BRIDGE);
                    app_restart_to_bridge();
                } else {
                    APPLED_ON(APP_CONFIG);
                    app_restart_to_webcfg();
                }
            }
            if (btn_long_press_cnt % 3 == 0) {
                ESP_LOGI(TAG, "Btn Press %d", btn_long_press_cnt);
            }
        } else {
            btn_long_press_cnt = 0;
        }
    }
}


/**
 * @brief
 *
 */
void app_main(void)
{
    /* 初始化任务间通信 */
    sema_config_post_finish = xSemaphoreCreateBinary();
    sema_config_err = xSemaphoreCreateBinary();
    sema_bridge_err = xSemaphoreCreateBinary();
    queue_runmode = xQueueCreate(1, sizeof(app_runmode_t));

    /* NVS初始化 */
    ESP_ERROR_CHECK(nvs_flash_init());


    /* 运行交互任务 */
    xTaskCreate((TaskFunction_t)hci_task, "hicTask", 1024 * 3, NULL, 1, NULL);

    /* 获取用户按键选择的运行应用 */
    app_runmode_t running_mode = app_get_rst_dist();
    xQueueSend(queue_runmode, &running_mode, portMAX_DELAY);

    /* 读取配置 */
    uint8_t cfg_ret = app_config_load(&global_cfg);

    if (running_mode == APP_CONFIG) {
        ESP_LOGI(TAG, "Web Config Start");
        appcfg_main();
    } else if (cfg_ret == CFG_LOAD_SUCCESS) {
        ESP_LOGI(TAG, "Read Config Success , Bridge Start");
        ESP_LOGI(TAG, "STA NAME:%s", global_cfg.sta_name);
        ESP_LOGI(TAG, "STA PASS:%s", global_cfg.sta_pass);
        ESP_LOGI(TAG, "AT NAME:%s", global_cfg.ap_name);
        ESP_LOGI(TAG, "AT PASS:%s", global_cfg.ap_pass);
        ESP_LOGI(TAG, "AT MAC:" MACSTR, MAC2STR(global_cfg.mac));
        bridge_main();
    }
}