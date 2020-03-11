//
// Created by cyber on 10.03.20.
//

#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <string.h>
#include <lvgl/src/lv_font/lv_symbol_def.h>

#include "esp32_settings.h"
#include "reppanel.h"

#define TAG "ESP32WiFi"
#define MAXIMUM_RETRY_WIFI  5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define DEFAULT_SCAN_LIST_SIZE  10

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY_WIFI) {
            reppanel_conn_status = 3;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        reppanel_conn_status = 2;
        ESP_LOGI(TAG, "Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got ip:"
        IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        reppanel_conn_status = 1;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    update_rep_panel_conn_status();
}

void wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, wifi_ssid, MAX_SSID_LEN);
    memcpy(wifi_config.sta.password, wifi_pass, MAX_WIFI_PASS_LEN);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void reconnect_wifi() {
    ESP_LOGI(TAG, "Reconnecting to %s with password %s", wifi_ssid, wifi_pass);
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, wifi_ssid, MAX_SSID_LEN);
    memcpy(wifi_config.sta.password, wifi_pass, MAX_WIFI_PASS_LEN);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    esp_wifi_connect();
}

void get_connection_info(char txt_buffer[200]) {
    wifi_ap_record_t ap_info;
    switch (reppanel_conn_status) {
        default:
        case 2:
        case 0:
            strcpy(txt_buffer, "Not connected");
            break;
        case 1:
            memset(&ap_info, 0, sizeof(ap_info));
            ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
            sprintf(txt_buffer, "Connected to %s\nSignal: %ddBm", ap_info.ssid, ap_info.rssi);
            break;
        case 3:
            strcpy(txt_buffer, "Reconnecting");
    }
}

void get_avail_wifi_networks(char *aps) {
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    if (ap_count > 0) {
        sprintf(aps, "%s", ap_info[0].ssid);
    }
    for (int i = 1; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        char tmp[50];
        sprintf(tmp, "\n%s", ap_info[i].ssid);
        strcat(aps, tmp);
        //ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        //ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        //ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
}