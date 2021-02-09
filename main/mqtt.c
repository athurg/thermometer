#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <lwip/sockets.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>
#include <cJSON.h>
#include <esp_log.h>
#include <sht3x.h>

#include "mqtt_client.h"

static char mqtt_addr[100] = CONFIG_MQTT_URI;
char *platform_create_id_string(void);
extern uint32_t sht3x_sn;
extern char mac_string[20];

static const char *TAG = "main.mqtt";
static esp_mqtt_client_handle_t client = NULL;

//数据上报间隔，默认10s
static bool mqtt_client_connected;
static uint32_t report_period_ms=10000;

esp_err_t mqtt_message_handler(cJSON *message)
{
	cJSON *cmd = cJSON_GetObjectItemCaseSensitive(message, "cmd");
	if (!cJSON_IsString(cmd) || cmd->valuestring == NULL) {
		ESP_LOGE(TAG, "Message cmd is empty");
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGE(TAG, "Get new cmd %s from MQTT", cmd->valuestring);

	if (strcmp("set_period", cmd->valuestring)) {
		ESP_LOGE(TAG, "Message type %s is not support", cmd->valuestring);
		return ESP_ERR_INVALID_ARG;
	}

	cJSON *value = cJSON_GetObjectItemCaseSensitive(message, "value");
	if (!cJSON_IsNumber(value) || value->valueint == 0) {
		ESP_LOGE(TAG, "Message value is empty");
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGE(TAG, "Get new cmd %s with value %d from MQTT", cmd->valuestring, value->valueint);

	report_period_ms = value->valueint;

	return ESP_OK;
}

esp_err_t mqtt_publish_data(void)
{
	esp_err_t ret;

	float temperature,humiture;
	ret = sht3x_get_humiture_periodic(&temperature,&humiture);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Fail to get Humiture failed");
		return ret;
	}

	ESP_LOGI(TAG,"temperature:%.2f °C, humidity:%.2f %%",temperature,humiture);

	cJSON *data = cJSON_CreateObject();
	cJSON_AddNumberToObject(data,"temperature",temperature);
	cJSON_AddNumberToObject(data,"humiture",humiture);

	cJSON *message = cJSON_CreateObject();   //创建根数据对象
	cJSON_AddStringToObject(message, "type", "report");
	cJSON_AddStringToObject(message, "mac", mac_string);
	cJSON_AddNumberToObject(message, "sn", sht3x_sn);
	cJSON_AddNumberToObject(message, "up", esp_log_early_timestamp());
	cJSON_AddItemToObject(message, "data", data);

	char *out = cJSON_Print(message);

	int msg_id = esp_mqtt_client_publish(client, "/sensor/temperature", out, 0, 0, 0);
	cJSON_free(out);

	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

	cJSON_Delete(message);

	return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	char topic[50];
	esp_mqtt_event_handle_t event = event_data;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			sprintf(topic, "/devices/%s", mac_string);
			esp_mqtt_client_subscribe(client, topic, 0);

			ESP_LOGW(TAG, "MQTT connect, subscribe to %s, and enable report loop", topic);

			mqtt_client_connected = true;
			break;

		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGW(TAG, "MQTT disconnect, disable report loop");
			mqtt_client_connected = false;
			break;

		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "TOPIC=%.*s DATA=%.*s", event->topic_len, event->topic, event->data_len, event->data);
			cJSON *message = cJSON_Parse(event->data);
			if (message == NULL) {
				ESP_LOGE("Invalid MQTT Message %s",cJSON_GetErrorPtr());
				break;
			}
			mqtt_message_handler(message);
			break;
		default:
			ESP_LOGW(TAG, "MQTT event %d", event->event_id);
			break;
	}
}

void mqtt_report_task(void *arg)
{
	esp_err_t ret;
	while(true) {
		vTaskDelay(report_period_ms / portTICK_PERIOD_MS);

		ESP_LOGI(TAG, "MQTT report loop");

		//打印Wi-Fi信息
		wifi_ap_record_t ap_info;
		ret = esp_wifi_sta_get_ap_info(&ap_info);
		if (ret!=ESP_OK){
			ESP_LOGE(TAG, "Get WiFi info failed %d", ret);
			continue;
		}
		ESP_LOGI(TAG, "WiFi connect to %s RSSI=%d", ap_info.ssid, ap_info.rssi);

		if (!mqtt_client_connected) {
			continue;
		}

		ret = mqtt_publish_data();
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Publish failed %d", ret);
			continue;
		}

		ESP_LOGI(TAG, "MQTT publish success");
	}
}

static void mqtt_app_start(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGW(TAG, "Start mqtt app");
	esp_mqtt_client_start(client);
}

static void mqtt_app_stop(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGW(TAG, "Stop mqtt app");
	esp_mqtt_client_stop(client);
}

static esp_err_t load_mqtt_param(void)
{
	esp_err_t ret;

	nvs_handle nvs_handle;
	ret = nvs_open("params", NVS_READWRITE, &nvs_handle);
	if (ret!=ESP_OK) {
		ESP_LOGE(TAG, "Fail to open NVS param handle %d", ret);
		return ret;
	}

	size_t length;
	static char buff[32];
	ret = nvs_get_str(nvs_handle, "mqtt_addr", buff, &length);
	if (ret != ESP_OK) {
		nvs_close(nvs_handle);
		ESP_LOGE(TAG, "Fail to read wifi_ssid from NVS param %d", ret);
		return ret;
	}

	if (strlen(buff)>0) {
		sprintf(mqtt_addr, "mqtt://%s", buff);
		ESP_LOGI(TAG, "Load mqtt server %s from NVS", mqtt_addr);
	}

	return ESP_OK;
}

void mqtt_app_init(void)
{
	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = mqtt_addr,
		.username = platform_create_id_string(),
		.password = platform_create_id_string(),
	};

	load_mqtt_param();

	ESP_LOGI(TAG, "Connect to mqtt %s", mqtt_cfg.uri);
	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, client);

	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &mqtt_app_start, NULL))
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &mqtt_app_stop, NULL))

	xTaskCreate(mqtt_report_task, "mqtt_report_task", 2048, NULL, 5, NULL);
}
