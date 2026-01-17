/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <sys/param.h>
//#include "nvs_flash.h"
#include "esp_netif.h"
//#include "esp_eth.h"
//#include "protocol_examples_common.h"
#include <cJSON.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include <esp_http_server.h>

#include "pages.h"
#include "bt_a2dp_sink.h"
#include "bt_globals.h"
#include "router_globals.h"

// 外部函数声明
extern int set_ap(int argc, char **argv);
extern int set_sta(int argc, char **argv);
extern int set_ap_mac(int argc, char **argv);
extern void preprocess_string(char* str);

// 强制门户相关定义
#define CAPTIVE_PORTAL_DOMAIN "captive.portal"

static const char *TAG = "HTTPServer";

// 函数声明
static esp_err_t modern_index_handler(httpd_req_t *req);
static esp_err_t config_post_handler(httpd_req_t *req);

esp_timer_handle_t restart_timer;

static void restart_timer_callback(void* arg)
{
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
}

esp_timer_create_args_t restart_timer_args = {
        .callback = &restart_timer_callback,
        /* argument specified here will be passed to timer callback function */
        .arg = (void*) 0,
        .name = "restart_timer"
};

/* An HTTP GET handler */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            if (strcmp(buf, "reset=Reboot") == 0) {
                esp_timer_start_once(restart_timer, 500000);
            }
            char param1[64];
            char param2[64];
            char param3[64];
            char param4[64];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "ap_ssid", param1, sizeof(param1)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => ap_ssid=%s", param1);
                preprocess_string(param1);
                if (httpd_query_key_value(buf, "ap_password", param2, sizeof(param2)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => ap_password=%s", param2);
                    preprocess_string(param2);
                    int argc = 3;
                    char* argv[3];
                    argv[0] = "set_ap";
                    argv[1] = param1;
                    argv[2] = param2;
                    set_ap(argc, argv);
                    esp_timer_start_once(restart_timer, 500000);
                }
            }
            if (httpd_query_key_value(buf, "ssid", param1, sizeof(param1)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => ssid=%s", param1);
                preprocess_string(param1);
                if (httpd_query_key_value(buf, "password", param2, sizeof(param2)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => password=%s", param2);
                    preprocess_string(param2);
                    if (httpd_query_key_value(buf, "ent_username", param3, sizeof(param3)) == ESP_OK) {
                        ESP_LOGI(TAG, "Found URL query parameter => ent_username=%s", param3);
                        preprocess_string(param3);
                        if (httpd_query_key_value(buf, "ent_identity", param4, sizeof(param4)) == ESP_OK) {
                            ESP_LOGI(TAG, "Found URL query parameter => ent_identity=%s", param4);
                            preprocess_string(param4);
                            int argc = 0;
                            char* argv[7];
                            argv[argc++] = "set_sta";
                            //SSID
                            argv[argc++] = param1;
                            //Password
                            argv[argc++] = param2;
                            //Username
                            if(strlen(param2)) {
                                argv[argc++] = "-u";
                                argv[argc++] = param3;
                            }
                            //Identity
                            if(strlen(param3)) {
                                argv[argc++] = "-a";
                                argv[argc++] = param4;
                            }
                            
                    set_sta(argc, argv);
                    esp_timer_start_once(restart_timer, 500000);
                        }
                    }
                }
            }
            if (httpd_query_key_value(buf, "staticip", param1, sizeof(param1)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => staticip=%s", param1);
                preprocess_string(param1);
                if (httpd_query_key_value(buf, "subnetmask", param2, sizeof(param2)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => subnetmask=%s", param2);
                    preprocess_string(param2);
                    if (httpd_query_key_value(buf, "gateway", param3, sizeof(param3)) == ESP_OK) {
                        ESP_LOGI(TAG, "Found URL query parameter => gateway=%s", param3);
                        preprocess_string(param3);
                        int argc = 4;
                        char* argv[4];
                        argv[0] = "set_sta_static";
                        argv[1] = param1;
                        argv[2] = param2;
                        argv[3] = param3;
                        set_sta_static(argc, argv);
                        esp_timer_start_once(restart_timer, 500000);
                    }
                }
            }
            // 处理蓝牙设置
            char bt_param[64];
            if (httpd_query_key_value(buf, "bt_enabled", bt_param, sizeof(bt_param)) == ESP_OK) {
                bool enabled = (atoi(bt_param) != 0);
                g_config.bluetooth.enabled = enabled;
                ESP_LOGI(TAG, "Bluetooth enabled: %s", enabled ? "true" : "false");
                
                if (httpd_query_key_value(buf, "bt_name", bt_param, sizeof(bt_param)) == ESP_OK) {
                    preprocess_string(bt_param);
                    if (strlen(bt_param) > 0 && strlen(bt_param) < 32) {
                        strcpy(g_config.bluetooth.device_name, bt_param);
                        ESP_LOGI(TAG, "Bluetooth name: %s", bt_param);
                    }
                }
                
                if (httpd_query_key_value(buf, "bt_volume", bt_param, sizeof(bt_param)) == ESP_OK) {
                    uint8_t volume = atoi(bt_param);
                    if (volume <= 100) {
                        g_config.bluetooth.volume = volume;
                        ESP_LOGI(TAG, "Bluetooth volume: %d%%", volume);
                    }
                }
                
                // 保存配置到NVS
                save_config_to_nvs();
                
                // 应用蓝牙配置
                bt_a2dp_sink_set_enabled(g_config.bluetooth.enabled);
                bt_a2dp_sink_set_name(g_config.bluetooth.device_name);
                bt_a2dp_sink_set_volume(g_config.bluetooth.volume);
            }
        }
        free(buf);
    }

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

/* 获取当前配置的API端点 */
static esp_err_t get_config_handler(httpd_req_t *req)
{
    char* ssid = NULL;
    char* passwd = NULL;
    char* ap_ssid = NULL;
    char* ap_passwd = NULL;

    // 从NVS获取配置
    esp_err_t err1 = get_config_param_str("ssid", &ssid);
    esp_err_t err2 = get_config_param_str("passwd", &passwd);
    esp_err_t err3 = get_config_param_str("ap_ssid", &ap_ssid);
    esp_err_t err4 = get_config_param_str("ap_passwd", &ap_passwd);

    // 创建JSON响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "ssid", err1 == ESP_OK ? ssid : "");
    cJSON_AddStringToObject(response, "passwd", err2 == ESP_OK ? passwd : "");
    cJSON_AddStringToObject(response, "ap_ssid", err3 == ESP_OK ? ap_ssid : "ESP32_Repeater");
    cJSON_AddStringToObject(response, "ap_passwd", err4 == ESP_OK ? ap_passwd : "12345678");
    cJSON_AddStringToObject(response, "ap_mac", ""); // 默认空
    
    // 添加蓝牙配置
    cJSON_AddBoolToObject(response, "bt_enabled", g_config.bluetooth.enabled);
    cJSON_AddStringToObject(response, "bt_name", g_config.bluetooth.device_name);
    cJSON_AddNumberToObject(response, "bt_volume", g_config.bluetooth.volume);

    char *response_string = cJSON_Print(response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, response_string, strlen(response_string));

    // 释放资源
    free(response_string);
    cJSON_Delete(response);
    if (ssid) free(ssid);
    if (passwd) free(passwd);
    if (ap_ssid) free(ap_ssid);
    if (ap_passwd) free(ap_passwd);

    return ESP_OK;
}

/* 处理配置POST请求的函数 */
static esp_err_t config_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    /* 读取POST数据 */
    ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received config data: %s", buf);

    /* 解析JSON */
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    bool config_updated = false;
    cJSON *response = cJSON_CreateObject();

    /* 处理STA配置 */
    cJSON *sta_ssid = cJSON_GetObjectItem(json, "sta_ssid");
    cJSON *sta_password = cJSON_GetObjectItem(json, "sta_password");

    if (cJSON_IsString(sta_ssid) && strlen(sta_ssid->valuestring) > 0) {
        char *ssid_str = strdup(sta_ssid->valuestring);
        char *passwd_str = cJSON_IsString(sta_password) ? strdup(sta_password->valuestring) : strdup("");

        preprocess_string(ssid_str);
        preprocess_string(passwd_str);

        int argc = 3;
        char* argv[3];
        argv[0] = "set_sta";
        argv[1] = ssid_str;
        argv[2] = passwd_str;

        if (set_sta(argc, argv) == ESP_OK) {
            ESP_LOGI(TAG, "STA config updated: %s", ssid_str);
            config_updated = true;
        }

        free(ssid_str);
        free(passwd_str);
    }

    /* 处理AP配置 */
    cJSON *ap_ssid = cJSON_GetObjectItem(json, "ap_ssid");
    cJSON *ap_password = cJSON_GetObjectItem(json, "ap_password");

    if (cJSON_IsString(ap_ssid) && strlen(ap_ssid->valuestring) > 0) {
        char *ap_ssid_str = strdup(ap_ssid->valuestring);
        char *ap_passwd_str = cJSON_IsString(ap_password) ? strdup(ap_password->valuestring) : strdup("");

        preprocess_string(ap_ssid_str);
        preprocess_string(ap_passwd_str);

        int argc = 3;
        char* argv[3];
        argv[0] = "set_ap";
        argv[1] = ap_ssid_str;
        argv[2] = ap_passwd_str;

        if (set_ap(argc, argv) == ESP_OK) {
            ESP_LOGI(TAG, "AP config updated: %s", ap_ssid_str);
            config_updated = true;
        }

        free(ap_ssid_str);
        free(ap_passwd_str);
    }

    /* 处理AP MAC地址配置 */
    cJSON *ap_mac = cJSON_GetObjectItem(json, "ap_mac");
    if (cJSON_IsString(ap_mac) && strlen(ap_mac->valuestring) > 0) {
        char *mac_str = strdup(ap_mac->valuestring);
        preprocess_string(mac_str);

        /* 解析MAC地址 */
        unsigned int mac_parts[6];
        if (sscanf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                   &mac_parts[0], &mac_parts[1], &mac_parts[2],
                   &mac_parts[3], &mac_parts[4], &mac_parts[5]) == 6) {

            int argc = 7;
            char* argv[7];
            char mac_args[6][4];

            argv[0] = "set_ap_mac";
            for (int i = 0; i < 6; i++) {
                sprintf(mac_args[i], "%u", mac_parts[i]);
                argv[i + 1] = mac_args[i];
            }

            if (set_ap_mac(argc, argv) == ESP_OK) {
                ESP_LOGI(TAG, "AP MAC updated: %s", mac_str);
                config_updated = true;
            }
        } else {
            ESP_LOGW(TAG, "Invalid MAC address format: %s", mac_str);
        }

        free(mac_str);
    }
    
    /* 处理蓝牙配置 */
    cJSON *bt_enabled = cJSON_GetObjectItem(json, "bt_enabled");
    cJSON *bt_name = cJSON_GetObjectItem(json, "bt_name");
    cJSON *bt_volume = cJSON_GetObjectItem(json, "bt_volume");
    
    bool bt_config_changed = false;
    
    if (bt_enabled != NULL) {
        bool enabled = false;
        if (cJSON_IsTrue(bt_enabled)) {
            enabled = true;
        } else if (cJSON_IsNumber(bt_enabled)) {
            enabled = (bt_enabled->valuedouble != 0);
        } else if (cJSON_IsString(bt_enabled)) {
            enabled = (atoi(bt_enabled->valuestring) != 0);
        }
        if (g_config.bluetooth.enabled != enabled) {
            g_config.bluetooth.enabled = enabled;
            bt_config_changed = true;
        }
    }
    
    if (cJSON_IsString(bt_name) && strlen(bt_name->valuestring) > 0 && strlen(bt_name->valuestring) < 32) {
        if (strcmp(g_config.bluetooth.device_name, bt_name->valuestring) != 0) {
            strcpy(g_config.bluetooth.device_name, bt_name->valuestring);
            bt_config_changed = true;
        }
    }
    
    if (bt_volume != NULL) {
        int volume = 0;
        if (cJSON_IsNumber(bt_volume)) {
            volume = (int)bt_volume->valuedouble;
        } else if (cJSON_IsString(bt_volume)) {
            volume = atoi(bt_volume->valuestring);
        }
        if (volume >= 0 && volume <= 100) {
            if (g_config.bluetooth.volume != (uint8_t)volume) {
                g_config.bluetooth.volume = (uint8_t)volume;
                bt_config_changed = true;
            }
        }
    }
    
    if (bt_config_changed) {
        // 保存蓝牙配置到NVS
        save_config_to_nvs();
        config_updated = true;
        
        // 应用蓝牙配置
        bt_a2dp_sink_set_enabled(g_config.bluetooth.enabled);
        bt_a2dp_sink_set_name(g_config.bluetooth.device_name);
        bt_a2dp_sink_set_volume(g_config.bluetooth.volume);
        
        ESP_LOGI(TAG, "蓝牙配置已更新: 启用=%d, 名称=%s, 音量=%d%%", 
                g_config.bluetooth.enabled, g_config.bluetooth.device_name, g_config.bluetooth.volume);
    }

    /* 发送响应 */
    if (config_updated) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Configuration saved successfully");

        /* 5秒后重启 */
        esp_timer_start_once(restart_timer, 5000000);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", "No valid configuration provided");
    }

    char *response_string = cJSON_Print(response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, response_string, strlen(response_string));

    free(response_string);
    cJSON_Delete(response);
    cJSON_Delete(json);

    return ESP_OK;
}



static httpd_uri_t config_post = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_post_handler,
};

static httpd_uri_t config_get = {
    .uri       = "/config",
    .method    = HTTP_GET,
    .handler   = get_config_handler,
};

/* 强制门户重定向处理器 */
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    const char* host_header = NULL;
    size_t host_len = httpd_req_get_hdr_value_len(req, "Host");

    if (host_len > 0) {
        char* host_value = malloc(host_len + 1);
        if (httpd_req_get_hdr_value_str(req, "Host", host_value, host_len + 1) == ESP_OK) {
            host_header = host_value;
        }
    }

    // 检查是否是对192.168.4.1的直接访问
    if (host_header && (strcmp(host_header, "192.168.4.1") == 0 ||
                       strcmp(host_header, "192.168.4.1:80") == 0)) {
        // 直接访问IP，返回配网页面
        if (host_header) free((void*)host_header);
        return modern_index_handler(req);
    }

    // 其他域名访问，重定向到配网页面
    ESP_LOGI(TAG, "Captive portal redirect for host: %s", host_header ? host_header : "unknown");

    if (host_header) free((void*)host_header);

    // 使用标准HTTP重定向
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

/* 通配符路由处理器 */
static esp_err_t wildcard_handler(httpd_req_t *req)
{
    // 检查User-Agent是否包含CaptiveNetworkSupport（iOS）或其他强制门户检测
    size_t user_agent_len = httpd_req_get_hdr_value_len(req, "User-Agent");
    if (user_agent_len > 0) {
        char* user_agent = malloc(user_agent_len + 1);
        if (httpd_req_get_hdr_value_str(req, "User-Agent", user_agent, user_agent_len + 1) == ESP_OK) {
            // iOS强制门户检测
            if (strstr(user_agent, "CaptiveNetworkSupport") != NULL) {
                free(user_agent);

                // 返回简单的成功页面，触发强制门户
                const char* success_page =
                    "<!DOCTYPE html><html><head><title>Success</title></head>"
                    "<body><script>window.location.href='http://192.168.4.1/';</script></body></html>";

                httpd_resp_set_type(req, "text/html");
                httpd_resp_send(req, success_page, strlen(success_page));
                return ESP_OK;
            }
        }
        free(user_agent);
    }

    // 默认重定向到配网页面
    return captive_portal_handler(req);
}

/* 处理各种操作系统的连接检测请求 */
static esp_err_t connectivity_check_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Connectivity check request: %s", req->uri);

    // 对于强制门户，重定向到配网页面而不是返回204
    // 这样可以更好地触发强制门户弹出
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

// 各种操作系统的连接检测URL
static httpd_uri_t generate_204 = {
    .uri       = "/generate_204",  // Android
    .method    = HTTP_GET,
    .handler   = connectivity_check_handler,
};

static httpd_uri_t hotspot_detect = {
    .uri       = "/hotspot-detect.html",  // iOS
    .method    = HTTP_GET,
    .handler   = captive_portal_handler,
};

static httpd_uri_t library_test = {
    .uri       = "/library/test/success.html",  // iOS
    .method    = HTTP_GET,
    .handler   = captive_portal_handler,
};

static httpd_uri_t ncsi_txt = {
    .uri       = "/ncsi.txt",  // Windows
    .method    = HTTP_GET,
    .handler   = connectivity_check_handler,
};

static httpd_uri_t connecttest = {
    .uri       = "/connecttest.txt",  // Windows 10
    .method    = HTTP_GET,
    .handler   = connectivity_check_handler,
};

static httpd_uri_t captive_portal = {
    .uri       = "/*",  // 通配符匹配所有路径
    .method    = HTTP_GET,
    .handler   = wildcard_handler,
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Page not found");
    return ESP_FAIL;
}

char* html_escape(const char* src) {
    //Primitive html attribue escape, should handle most common issues.
    int len = strlen(src);
    //Every char in the string + a null
    int esc_len = len + 1;

    for (int i = 0; i < len; i++) {
        if (src[i] == '\\' || src[i] == '\'' || src[i] == '\"' || src[i] == '&' || src[i] == '#' || src[i] == ';') {
            //Will be replaced with a 5 char sequence
            esc_len += 4;
        }
    }

    char* res = malloc(sizeof(char) * esc_len);

    int j = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == '\\' || src[i] == '\'' || src[i] == '\"' || src[i] == '&' || src[i] == '#' || src[i] == ';') {
            res[j++] = '&';
            res[j++] = '#';
            res[j++] = '0' + (src[i] / 10);
            res[j++] = '0' + (src[i] % 10);
            res[j++] = ';';
        }
        else {
            res[j++] = src[i];
        }
    }
    res[j] = '\0';

    return res;
}

// DNS服务器功能已移除，使用阿里云DNS替代

/* 新的index处理器，直接提供现代化的配网页面 */
static esp_err_t modern_index_handler(httpd_req_t *req)
{
    extern const char index_html_start[] asm("_binary_index_html_start");
    extern const char index_html_end[] asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, index_html_start, index_html_size);
    return ESP_OK;
}

static httpd_uri_t modern_index = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = modern_index_handler,
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    esp_timer_create(&restart_timer_args, &restart_timer);

    // DNS服务器已移除，使用阿里云DNS替代

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers - 注意顺序很重要，具体路径要在通配符之前
        ESP_LOGI(TAG, "Registering URI handlers");

        // 主要功能页面
        httpd_register_uri_handler(server, &modern_index);
        httpd_register_uri_handler(server, &config_get);
        httpd_register_uri_handler(server, &config_post);

        // 各种操作系统的连接检测URL
        httpd_register_uri_handler(server, &generate_204);    // Android
        httpd_register_uri_handler(server, &hotspot_detect);  // iOS
        httpd_register_uri_handler(server, &library_test);    // iOS
        httpd_register_uri_handler(server, &ncsi_txt);        // Windows
        httpd_register_uri_handler(server, &connecttest);     // Windows 10

        // 注册强制门户通配符处理器（必须最后注册）
        httpd_register_uri_handler(server, &captive_portal);

        ESP_LOGI(TAG, "Captive portal enabled - all requests will redirect to config page");
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // DNS服务器已移除，无需停止

    // Stop the httpd server
    httpd_stop(server);
}
