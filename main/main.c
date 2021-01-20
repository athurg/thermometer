/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <protocol_examples_common.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <sht3x.h>

#include "webserver.h"
#include "mqtt.h"

//日志标签
static const char *TAG="MAIN";

//WiFi断开事件处理函数
static void station_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	ESP_LOGI(TAG, "WiFi station was disconnected");
	ESP_LOGI(TAG, "Starting webserver");
	stop_webserver();
}

//WiFi获取到IP事件处理函数
static void station_got_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	ESP_LOGI(TAG, "WiFi station got new ip");
	ESP_LOGI(TAG, "Stop webserver");
	start_webserver();
}

void app_main()
{
	//系统层初始化，失败直接panic
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

	//用户层初始化
	esp_err_t ret;

	ret = sht3x_mode_init();
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to init SHT3X: %X", ret);
	}

	//Connect WiFi
	ret = example_connect();
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to connect WiFi: %X", ret);
	}

    mqtt_app_start();

	ret = start_webserver();
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to start webserver: %X", ret);
	}

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &station_got_ip_handler, NULL);
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to register IP_EVENT_STA_GOT_IP handler: %X", ret);
	}

    ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &station_disconnect_handler, NULL);
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to register WIFI_EVENT_STA_DISCONNECTED handler: %X", ret);
	}
}
