#include "apcfg_main.h"
#include "common.h"
#include "esp_log.h"
#include "nvs_flash.h"


static const char *TAG = "Main";
app_config_t global_cfg;

void app_main(void)
{
    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    
    uint8_t ret = app_config_load(&global_cfg);
    if (ret == CFG_LOAD_FAIL)
    {
        ESP_LOGI(TAG, "Read Config Fail , Web Config Start");
        appcfg_main();
    }else{
        ESP_LOGI(TAG, "Read Config Success , Bridge Start");
    }
}