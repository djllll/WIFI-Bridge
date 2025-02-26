#include "common.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"

/**
 * @brief
 *
 * @param config
 */
void app_config_save(app_config_t *config)
{
    nvs_handle_t config_nvs_handle;
    config->sumcheck = 0;
    for (uint16_t i = 0; i < CFG_STR_LEN; i++) {
        config->sumcheck = (uint8_t)(((uint16_t)config->sumcheck + config->ap_name[i]) & 0xff);
        config->sumcheck = (uint8_t)(((uint16_t)config->sumcheck + config->ap_pass[i]) & 0xff);
        config->sumcheck = (uint8_t)(((uint16_t)config->sumcheck + config->sta_name[i]) & 0xff);
        config->sumcheck = (uint8_t)(((uint16_t)config->sumcheck + config->sta_pass[i]) & 0xff);
    }
    nvs_open("APPCFG", NVS_READWRITE, &config_nvs_handle);
    nvs_set_blob(config_nvs_handle, "CFG", (const void *)config, sizeof(app_config_t));
    nvs_close(config_nvs_handle);
}


/**
 * @brief
 *
 * @param config
 * @return uint8_t
 */
uint8_t app_config_load(app_config_t *config)
{
    nvs_handle_t config_nvs_handle;
    nvs_open("APPCFG", NVS_READWRITE, &config_nvs_handle);
    uint16_t len = sizeof(app_config_t);
    nvs_get_blob(config_nvs_handle, "CFG", (void *)config, &len);
    nvs_close(config_nvs_handle);

    uint8_t sumcheck = 0;
    for (uint16_t i = 0; i < CFG_STR_LEN; i++) {
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->ap_name[i]) & 0xff);
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->ap_pass[i]) & 0xff);
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->sta_name[i]) & 0xff);
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->sta_pass[i]) & 0xff);
    }
    return sumcheck == config->sumcheck ? CFG_LOAD_SUCCESS : CFG_LOAD_FAIL;
}


uint8_t hci_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((1UL << LEDG_GPIO) | (1UL << LEDB_GPIO));
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1UL << BTN_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    return 0;
}


uint8_t hci_deinit(void)
{

    return 0;
}


void app_restart_to_webcfg(void)
{
    nvs_handle_t nvs_handle;
    nvs_open("APPCFG", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "RST", (uint8_t)APP_CONFIG);
    nvs_close(nvs_handle);
    esp_restart();
}
void app_restart_to_bridge(void)
{
    nvs_handle_t nvs_handle;
    nvs_open("APPCFG", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "RST", (uint8_t)APP_BRIDGE);
    nvs_close(nvs_handle);
    esp_restart();
}

app_runmode_t app_get_rst_dist(void)
{
    app_runmode_t rst;
    nvs_handle_t nvs_handle;
    nvs_open("APPCFG", NVS_READWRITE, &nvs_handle);
    nvs_get_u8(nvs_handle, "RST", (uint8_t *)&rst);
    nvs_close(nvs_handle);
    return rst==APP_BRIDGE?APP_BRIDGE:APP_CONFIG;
}