#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "system_status.h"

#define WIFI_MAXIMUM_RETRY 5

static const char *TAG = "wifi_manager";

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif;
static esp_event_handler_instance_t s_wifi_handler_instance;
static esp_event_handler_instance_t s_ip_handler_instance;
static bool s_wifi_initialized;
static int s_retry_num;
static char s_configured_ssid[33];

enum {
    WIFI_CONNECTED_BIT = BIT0,
    WIFI_FAIL_BIT = BIT1,
};

static esp_err_t ignore_already_initialized(esp_err_t err)
{
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
}

static esp_err_t wifi_manager_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        ret = nvs_flash_init();
    }
    return ret;
}

static void wifi_manager_load_ssid(void)
{
    wifi_config_t wifi_config = {0};

    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
        s_configured_ssid[0] = '\0';
        return;
    }

    strlcpy(s_configured_ssid, (const char *) wifi_config.sta.ssid, sizeof(s_configured_ssid));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void) arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        system_status_set_device_state(DEVICE_STATE_WIFI_CONNECTING);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        system_status_set_wifi(s_configured_ssid, NULL, false);
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying Wi-Fi connection (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            system_status_set_device_state(DEVICE_STATE_WIFI_FAILED);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        char ip_text[16];

        s_retry_num = 0;
        snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&event->ip_info.ip));
        system_status_set_wifi(s_configured_ssid, ip_text, true);
        system_status_set_device_state(DEVICE_STATE_WIFI_CONNECTED);
        ESP_LOGI(TAG, "Connected with IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_RETURN_ON_ERROR(wifi_manager_init_nvs(), TAG, "nvs init failed");
    ESP_RETURN_ON_ERROR(ignore_already_initialized(esp_netif_init()), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(ignore_already_initialized(esp_event_loop_create_default()), TAG, "event loop init failed");

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(s_wifi_event_group != NULL, ESP_ERR_NO_MEM, TAG, "event group create failed");
    }

    if (s_sta_netif == NULL) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        ESP_RETURN_ON_FALSE(s_sta_netif != NULL, ESP_FAIL, TAG, "create STA netif failed");
    }

    if (!s_wifi_initialized) {
        const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
        s_wifi_initialized = true;
    }

    if (s_wifi_handler_instance == NULL) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
                                                &s_wifi_handler_instance),
            TAG, "register WIFI handler failed");
    }
    if (s_ip_handler_instance == NULL) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
                                                &s_ip_handler_instance),
            TAG, "register IP handler failed");
    }

    wifi_manager_load_ssid();
    return ESP_OK;
}

bool wifi_manager_is_provisioned(void)
{
    wifi_config_t wifi_config = {0};

    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
        return false;
    }

    return wifi_config.sta.ssid[0] != '\0';
}

esp_err_t wifi_manager_start_sta(void)
{
    wifi_manager_load_ssid();
    ESP_RETURN_ON_FALSE(s_configured_ssid[0] != '\0', ESP_ERR_INVALID_STATE, TAG,
                        "no saved Wi-Fi config");

    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;
    system_status_set_wifi(s_configured_ssid, NULL, false);
    system_status_set_device_state(DEVICE_STATE_WIFI_CONNECTING);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    ESP_LOGI(TAG, "Wi-Fi STA started. SSID=%s", s_configured_ssid);
    return ESP_OK;
}

bool wifi_manager_wait_connected(uint32_t timeout_ms)
{
    const EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    }

    if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Wi-Fi connection failed");
    } else {
        ESP_LOGE(TAG, "Timed out waiting for Wi-Fi connection");
    }

    system_status_set_device_state(DEVICE_STATE_WIFI_FAILED);
    return false;
}

esp_err_t wifi_manager_reset_config(void)
{
    system_status_set_device_state(DEVICE_STATE_RESETTING_WIFI);
    esp_wifi_disconnect();
    esp_wifi_stop();
    ESP_RETURN_ON_ERROR(esp_wifi_restore(), TAG, "restore Wi-Fi config failed");
    s_configured_ssid[0] = '\0';
    system_status_set_wifi("", NULL, false);
    ESP_LOGI(TAG, "Wi-Fi config cleared");
    return ESP_OK;
}
