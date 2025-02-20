#ifndef _COMMON_H_
#define _COMMON_H_

#include "stdint.h"

#define RUNMODE_WEB_CFG 0
#define RUNMODE_BRIDGE 1
#define CFG_LOAD_SUCCESS 0
#define CFG_LOAD_FAIL 1
#define CFG_STR_LEN 33

#define LED_GPIO 4
#define BTN_GPIO 2

#define RST_TO_WEBCFG 0xa1
#define RST_TO_BRIDGE 0xa2

typedef struct{
    char sta_name[CFG_STR_LEN];
    char sta_pass[CFG_STR_LEN];
    char ap_name[CFG_STR_LEN];
    char ap_pass[CFG_STR_LEN];
    uint8_t mac[6];
    uint8_t sumcheck;
}app_config_t;


uint8_t hci_init(void);
uint8_t hci_deinit(void);

void app_config_save(app_config_t *config);
uint8_t app_config_load(app_config_t *config);
void app_restart_to_webcfg(void);
void app_restart_to_bridge(void);
uint8_t app_get_rst_dist(void);

#endif