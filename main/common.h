#ifndef _COMMON_H_
#define _COMMON_H_

#include "stdint.h"

typedef struct{
    char sta_name[32];
    char sta_pass[32];
    char ap_name[32];
    char ap_pass[32];
    uint8_t mac[6];
}app_config_t;



void app_config_save(app_config_t *config);
void app_config_load(app_config_t *config);

#endif