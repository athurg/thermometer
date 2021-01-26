#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <protocol_examples_common.h>
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

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
extern uint32_t sht3x_sn;
extern char mac_string[20];

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
	cJSON_AddItemToObject(message, "data", data);

	char *out = cJSON_Print(message);

	int msg_id = esp_mqtt_client_publish(client, "/sensor/temperature", out, 0, 1, 0);
	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

	cJSON_Delete(message);

	return ESP_OK;
}

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_mqtt_event_handle_t event = event_data;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA TOPIC=%.*s DATA=%.*s", event->topic_len, event->topic, event->data_len, event->data);
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			break;
		default:
			ESP_LOGI(TAG, "MQTT_EVENT_UNKNOWN(%d)", event->event_id);
			break;
	}
}

void mqtt_report_task(void *arg)
{
	while(1) {
		ESP_LOGI(TAG, "Report Loop");
		mqtt_publish_data();
		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}

void mqtt_app_start(void)
{
	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = CONFIG_MQTT_URI,
#ifdef CONFIG_MQTT_USERNAME
		.username = CONFIG_MQTT_USERNAME,
#endif

#ifdef CONFIG_MQTT_PASSWORD
		.password = CONFIG_MQTT_PASSWORD,
#endif
	};

	client = esp_mqtt_client_init(&mqtt_cfg);

	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, client);

	esp_mqtt_client_start(client);

	xTaskCreate(mqtt_report_task, "mqtt_report_task", 2048, NULL, 5, NULL);
}
