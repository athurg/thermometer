#include <string.h>
#include "esp_log.h"
#include "lwip/apps/sntp.h"

static const char *TAG = "main.sntp";
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp1.aliyun.com");
    sntp_init();
}

#define SECONDS_OF_ONE_YEAR 365*24*60*50
void wait_time_sync(void)
{
    initialize_sntp();

	//等待NTP同步完成
    int retry = 0;
    time_t timestamp = 0;
    while (1) {
        time(&timestamp);
		if (timestamp > SECONDS_OF_ONE_YEAR) {
			break;
		}

		ESP_LOGI(TAG, "Waiting (%d) more 3s for time sync (current timestamp=%ld) ...", ++retry, timestamp);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

	//获取本地时间
    struct tm timeinfo;
    setenv("TZ", "CST-8", 1);
    tzset();
	localtime_r(&timestamp, &timeinfo);

	//打印
    char strftime_buf[20];
	strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
	ESP_LOGI(TAG, "Time synced, current time is: %s", strftime_buf);
}
