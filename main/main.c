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

#include "mqtt.h"
#include "time.h"

//日志标签
static const char *TAG="MAIN";

uint32_t sht3x_sn;
char mac_string[20];

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGW(TAG, "Wi-Fi event %d", event_id);
	if(event_id==WIFI_EVENT_STA_DISCONNECTED) {
		mqtt_app_stop();
	}
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGW(TAG, "IP event %d", event_id);
	if (event_id==IP_EVENT_STA_GOT_IP) {
		mqtt_app_start();
	}
}


void app_main()
{
	//用户层初始化
	esp_err_t ret;

	//系统层初始化，失败直接panic
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

	//芯片初始化
	ret = sht3x_mode_init();
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to init SHT3X: %X", ret);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
		esp_restart();
	}

	vTaskDelay(1000 / portTICK_PERIOD_MS);
	//读取传感器序列号
	ret = SHT3x_ReadSerialNumber(&sht3x_sn);
	if(ret != ESP_OK) {
		ESP_LOGE(TAG,"Read SerialNumber failed");
		vTaskDelay(3000 / portTICK_PERIOD_MS);
		esp_restart();
	}

	ESP_LOGI(TAG, "Sensor SHT3X SN=0x%x", sht3x_sn);

	//读取MAC地址并转换成字符串
	uint8_t mac_buffer[6];
	esp_efuse_mac_get_default(mac_buffer);
	sprintf(mac_string, "%02X:%02X:%02X:%02X:%02X:%02X", mac_buffer[0],mac_buffer[1],mac_buffer[2],mac_buffer[3],mac_buffer[4],mac_buffer[5]);
	ESP_LOGI(TAG, "MAC address %s", mac_string);

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_disconnect, NULL))
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_got_ip, NULL))

    mqtt_app_init();

	//所有初始化完成方可联网
	ret = example_connect();
	if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to connect WiFi: %X", ret);
	}

	wait_time_sync();
}
