#include "common.h"
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
    for (uint16_t i = 0; i < CFG_STR_LEN; i++)
    {
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
    for (uint16_t i = 0; i < CFG_STR_LEN; i++)
    {
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->ap_name[i]) & 0xff);
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->ap_pass[i]) & 0xff);
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->sta_name[i]) & 0xff);
        sumcheck = (uint8_t)(((uint16_t)sumcheck + config->sta_pass[i]) & 0xff);
    }
    return sumcheck == config->sumcheck ? CFG_LOAD_SUCCESS : CFG_LOAD_FAIL;
}