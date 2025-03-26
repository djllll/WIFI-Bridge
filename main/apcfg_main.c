/* Captive Portal Example

    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sys/param.h>
#include "apcfg_main.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"

#include "common.h"


#define ESP_WIFI_SSID "esp-bridge-"
#define ESP_WIFI_PASS "12345678"
#define MAX_STA_CONN 2

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");
static const char *TAG = "dns_server";

extern  SemaphoreHandle_t sema_config_post_finish;
extern SemaphoreHandle_t sema_config_err;

extern app_config_t global_cfg;
 
#define CFG_ERR_CHECK(_f)                            \
    if (_f != ESP_OK) {                                 \
        xSemaphoreGive(sema_config_err); \
        while (1) {                                     \
            ESP_LOGW(TAG, "appconfig_check_error!");    \
            vTaskDelay(1000);                            \
        };                                              \
    }


/**
 * @brief WIFI 事件处理
 * 
 * @param arg 
 * @param event_base 
 * @param event_id 
 * @param event_data 
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}


/**
 * @brief ap初始化
 * 
 */
static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    CFG_ERR_CHECK(esp_wifi_init(&cfg));

    CFG_ERR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    uint8_t fuse_mac[6];
    CFG_ERR_CHECK(esp_base_mac_addr_get(fuse_mac));
    uint16_t only_postfix = 0;
    for (uint8_t i = 0; i < 6; i++) {
        only_postfix += fuse_mac[i];
    }


    sprintf((char*)wifi_config.ap.ssid,ESP_WIFI_SSID"%04d",only_postfix);
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);

    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    CFG_ERR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    CFG_ERR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    CFG_ERR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);
}


/**
 * @brief 解析键值对
 * 
 * @param input 
 * @param key 
 * @param value 
 */
void parse_key_value(const char *input, char *key, char *value) {
    const char *equal_pos = strchr(input, '=');
    if (equal_pos != NULL) {
        size_t key_len = equal_pos - input;
        strncpy(key, input, key_len);
        key[key_len] = '\0';
        strcpy(value, equal_pos + 1);
    }
}



/**
 * @brief 根url返回网页
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;
    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, root_len);
    return ESP_OK;
}


/**
 * @brief 查询当前配置数据
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t config_get_handler(httpd_req_t *req)
{
    char ret[512];
    int len = sprintf(ret, "{ \
    \"sta_name\":\"%s\", \
    \"sta_pass\":\"%s\", \
    \"ap_name\":\"%s\", \
    \"ap_pass\":\"%s\", \
    \"ap_mac\":\"" MACSTR "\" \
    }",
            global_cfg.sta_name, global_cfg.sta_pass, global_cfg.ap_name, global_cfg.ap_pass, MAC2STR(global_cfg.mac));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ret, len);
    return ESP_OK;
}

// 字符替换函数
static void replace_char(char *str, char old_char, char new_char) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == old_char) {
            str[i] = new_char;
        }
    }
}

/**
 * @brief 处理post表单提交
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t root_post_handler(httpd_req_t *req)
{
    char buf[1024];
    size_t recv_size = MIN(req->content_len, sizeof(buf));

    // 读取POST请求的内容
    int ret = httpd_req_recv(req, buf, recv_size);
    if (ret <= 0) {  // 错误处理
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    buf[ret] = '\0';

    // 解析表单数据
    app_config_t cfg;
    char *token = strtok(buf, "&");
    uint8_t mac_idx=0;
    while (token != NULL) {
        char key[CFG_STR_LEN];
        char value[CFG_STR_LEN];
        parse_key_value(token, key, value);
        ESP_LOGI(TAG, "Key: %s, Value: %s", key, value);
        if(strcmp((const char *)key,"sta_name")==0){
            strcpy(cfg.sta_name,(const char*)value);
        }else if(strcmp((const char *)key,"sta_pass")==0){
            strcpy(cfg.sta_pass,(const char*)value);
        }else if(strcmp((const char *)key,"ap_name")==0){
            strcpy(cfg.ap_name,(const char*)value);
        }else if(strcmp((const char *)key,"ap_pass")==0){
            strcpy(cfg.ap_pass,(const char*)value);
        }else if(strncmp((const char *)key,"ap_mac",6)==0){
            cfg.mac[mac_idx] = strtol(value, NULL, 16);
            mac_idx++;
        }else{
            ESP_LOGW(TAG,"Match Unkown");
        }
        token = strtok(NULL, "&");
    }

    // 提交上来的ssid的空格会被转义为+ 还原
    replace_char(cfg.sta_name,'+',' ');
    replace_char(cfg.ap_name,'+',' ');

    app_config_save(&cfg);



    // 发送响应
    const char *resp_str = "Configure Success, Press and hold the button to switch the mode.";
    httpd_resp_send(req, resp_str, strlen(resp_str));

    xSemaphoreGive(sema_config_post_finish);

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};


static const httpd_uri_t root_post = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = root_post_handler
};


static const httpd_uri_t config_get = {
    .uri = "/config",
    .method = HTTP_GET,
    .handler = config_get_handler
};


// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &root_post);
        httpd_register_uri_handler(server, &config_get);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

void appcfg_main(void)
{
    /*
        Turn of warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);


    // Initialize networking stack
    CFG_ERR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    CFG_ERR_CHECK(esp_event_loop_create_default());



    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&config);
}
