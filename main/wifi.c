#include <string.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <esp_log.h>

#include "web.h"

static char wifi_ssid[32] = CONFIG_WIFI_DEFAULT_SSID;
static char wifi_passwd[32] = CONFIG_WIFI_DEFAULT_PASSWORD;

static const char *TAG = "main.wifi";

#define MAX_RECONNECT 5
static int connect_count = 0;

static void on_wifi_connect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "Wi-Fi connected");
	connect_count = 0;
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *)event_data;

    ESP_LOGW(TAG, "Wi-Fi disconnected (reason=%d), trying to reconnect...", event->reason);

	// Switch to 802.11 bgn mode
    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    } else {
		connect_count ++;
	}

	//多次重试失败后切换到AP模式
	if (connect_count>MAX_RECONNECT) {
		ESP_LOGW(TAG, "Wi-Fi connected failed more than %d times, switch to AP mode...", connect_count);
		ESP_ERROR_CHECK(esp_wifi_stop());
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		ESP_ERROR_CHECK(esp_wifi_start());
		ESP_ERROR_CHECK(start_webserver());
		return;
	}

	//自动重连
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got new ip: " IPSTR, IP2STR(&event->ip_info.ip));
}

static esp_err_t load_wifi_param(void)
{
	esp_err_t ret;

	nvs_handle nvs_handle;
	ret = nvs_open("params", NVS_READWRITE, &nvs_handle);
	if (ret!=ESP_OK) {
		ESP_LOGE(TAG, "Fail to open NVS param handle %d", ret);
		return ret;
	}

	size_t length;
	ret = nvs_get_str(nvs_handle, "wifi_ssid", wifi_ssid, &length);
	if (ret != ESP_OK) {
		nvs_close(nvs_handle);
		ESP_LOGE(TAG, "Fail to read wifi_ssid from NVS param %d", ret);
		return ret;
	}

	ret = nvs_get_str(nvs_handle, "wifi_pass", wifi_passwd, &length);
	if (ret != ESP_OK) {
		nvs_close(nvs_handle);
		ESP_LOGE(TAG, "Fail to read wifi_pass from NVS param %d", ret);
		return ret;
	}

	return ESP_OK;
}

esp_err_t wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = { 0 };

	load_wifi_param();

    strncpy((char *)&wifi_config.sta.ssid, wifi_ssid, 32);
    strncpy((char *)&wifi_config.sta.password, wifi_passwd, 32);

    ESP_LOGI(TAG, "Connecting to %s with %s...", wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    return ESP_OK;
}
