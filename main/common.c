#include "common.h"
#include "nvs_flash.h"

void app_config_save(app_config_t *config){
    nvs_handle_t config_nvs_handle;
    nvs_open("APPCFG", NVS_READWRITE, &config_nvs_handle);
    nvs_set_blob(config_nvs_handle,"CFG",config,sizeof(app_config_t));
}


void app_config_load(app_config_t *config){
    nvs_handle_t config_nvs_handle;
    nvs_open("APPCFG", NVS_READWRITE, &config_nvs_handle);
    nvs_get_blob(config_nvs_handle,"CFG",config,sizeof(app_config_t));
}