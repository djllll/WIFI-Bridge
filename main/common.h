#ifndef _COMMON_H_
#define _COMMON_H_

#include "stdint.h"

#define RUNMODE_WEB_CFG 0
#define RUNMODE_BRIDGE 1
#define CFG_LOAD_SUCCESS 0
#define CFG_LOAD_FAIL 1

#define CFG_STR_LEN 33

typedef struct{
    char sta_name[CFG_STR_LEN];
    char sta_pass[CFG_STR_LEN];
    char ap_name[CFG_STR_LEN];
    char ap_pass[CFG_STR_LEN];
    uint8_t mac[6];
    uint8_t sumcheck;
}app_config_t;



void app_config_save(app_config_t *config);
uint8_t app_config_load(app_config_t *config);

#endif