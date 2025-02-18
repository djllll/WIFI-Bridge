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

#define ESP_WIFI_SSID "esp-bridge-config"
#define ESP_WIFI_PASS "12345678"
#define MAX_STA_CONN 2

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");
static const char *TAG = "dns_server";

extern  SemaphoreHandle_t sema_restart_to_bridge;


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

static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             ESP_WIFI_SSID, ESP_WIFI_PASS);
}

// 解析键值对
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
 * @brief mac地址解析
 * 
 * @param mac_str 
 * @param mac_array 
 * @return uint8_t 
 */
uint8_t mac_string_to_array(const char *mac_str, uint8_t mac_array[6]) {
    uint8_t ret = sscanf(mac_str, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
                     &mac_array[0], &mac_array[1], &mac_array[2],
                     &mac_array[3], &mac_array[4], &mac_array[5]);
    // 如果成功解析了6个十六进制值，则返回1表示成功
    return (ret == 6) ? 1 : 0;
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;
    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, root_len);
    return ESP_OK;
}

// HTTP POST Handler
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
        }else if(strcmp((const char *)key,"ap_mac")==0){
            mac_string_to_array(value,cfg.mac);
        }else{
            ESP_LOGW(TAG,"Match Unkown");
        }
        token = strtok(NULL, "&");
    }
    app_config_save(&cfg);



    // 发送响应
    const char *resp_str = "Reboot...";
    httpd_resp_send(req, resp_str, strlen(resp_str));

    xSemaphoreGive(sema_restart_to_bridge);

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
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());



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
