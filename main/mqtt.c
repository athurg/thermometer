#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include <cJSON.h>

#include "esp_log.h"
#include "mqtt_client.h"
#include "sht3x.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;

esp_err_t register_device()
{
	uint8_t mac[6];
	memset(mac, 0, sizeof(mac));
	esp_efuse_mac_get_default(mac);

    char mac_string[20];
	sprintf(mac_string, "%X:%X:%X:%X:%X:%X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

	cJSON *message = cJSON_CreateObject();   //创建根数据对象
	cJSON_AddStringToObject(message,"device_type", "temperature");
	cJSON_AddStringToObject(message,"device_model", "SHT30");
	cJSON_AddStringToObject(message,"device_id",mac_string);

	char *buff = cJSON_Print(message);
	int msg_id = esp_mqtt_client_publish(client, "/iot/register", buff, 0, 1, 0);
	ESP_LOGI(TAG, "Register device to /iot/register successful, msg_id=%d", msg_id);

	cJSON_Delete(message);

	//订阅设备专有主题
	char topic[100];
	sprintf(topic, "/device/%X:%X:%X:%X:%X:%X/cmd", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	msg_id = esp_mqtt_client_subscribe(client, topic, 0);
	ESP_LOGI(TAG, "sent subscribe %s successful, msg_id=%d", topic, msg_id);

	return ESP_OK;
}

esp_err_t mqtt_publish_sample()
{
	esp_err_t ret;

	uint32_t serial_number;
	ret = SHT3x_ReadSerialNumber(&serial_number);
	if(ret == ESP_OK) {
        ESP_LOGI(TAG, "Read SerialNumber ok: 0x%x", serial_number);
	} else {
		ESP_LOGE(TAG,"Read SerialNumber failed");
	}

    float temperature,humiture;
	ret = sht3x_get_humiture_periodic(&temperature,&humiture);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG,"temperature:%.2f °C, humidity:%.2f %%",temperature,humiture);
	} else {
		ESP_LOGE(TAG,"Fail to get Humiture failed");
	}

	cJSON *message = cJSON_CreateObject();   //创建根数据对象
	cJSON_AddNumberToObject(message,"sn",serial_number);
	cJSON_AddNumberToObject(message,"temperature",temperature);
	cJSON_AddNumberToObject(message,"humiture",humiture);
	char *out = cJSON_Print(message);

	int msg_id = esp_mqtt_client_publish(client, "/iot/sht3x", out, 0, 1, 0);
	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

	cJSON_Delete(message);

	return ESP_OK;
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			register_device();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

			mqtt_publish_sample();
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://gcp.gooth.org",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}
