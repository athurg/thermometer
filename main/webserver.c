#include <esp_log.h>
#include <esp_err.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <sht3x.h>

//日志标签
static const char *TAG="WEBSERVER";

httpd_handle_t server = NULL;

typedef enum{
	PROMETHEUS_METRIC_CATEGORY_INVALID = 0,
	PROMETHEUS_METRIC_CATEGORY_COUNTER = 1,
	PROMETHEUS_METRIC_CATEGORY_GAUGE = 2,
}prometheus_metric_category_t;

typedef struct {
	char *name;
	char *help;
	float value;
	prometheus_metric_category_t category;
}prometheus_metric_t;

//序列号指标
prometheus_metric_t metric_sn = {
	.value = 0,
	.name = "serial_number",
	.help = "Serial number of SHT3x",
	.category = PROMETHEUS_METRIC_CATEGORY_GAUGE
};

//温度指标
prometheus_metric_t metric_temperature = {
	.value = 0,
	.name = "temperature_celsius",
	.help = "Temperature as celsius",
	.category = PROMETHEUS_METRIC_CATEGORY_GAUGE
};

//相对湿度指标
prometheus_metric_t metric_humiture = {
	.value = 0,
	.name = "humiture",
	.help = "Humiture",
	.category = PROMETHEUS_METRIC_CATEGORY_GAUGE
};


//最少20个字节的buff
esp_err_t httpd_resp_send_prometheus_metric(httpd_req_t *req, prometheus_metric_t metric) {
	httpd_resp_send_chunk(req, "# HELP ", -1);
	httpd_resp_send_chunk(req, metric.name, -1);
	httpd_resp_send_chunk(req, " ", -1);
	httpd_resp_send_chunk(req, metric.help, -1);
	httpd_resp_send_chunk(req, "\n", -1);

	httpd_resp_send_chunk(req, "# TYPE ", -1);
	httpd_resp_send_chunk(req, metric.name, -1);
	httpd_resp_send_chunk(req, " ", -1);
	switch(metric.category) {
		case PROMETHEUS_METRIC_CATEGORY_GAUGE:
			httpd_resp_send_chunk(req, "gauge", -1);
			break;
		case PROMETHEUS_METRIC_CATEGORY_COUNTER:
			httpd_resp_send_chunk(req, "counter", -1);
			break;
		default:
			return ESP_ERR_INVALID_ARG;
	}
	httpd_resp_send_chunk(req, "\n", -1);

	//获取MAC地址
	uint8_t mac[6];
	memset(mac, 0, sizeof(mac));
	esp_efuse_mac_get_default(mac);

    char resp[256];
	sprintf(resp, "%s{mac=\"%X:%X:%X:%X:%X:%X\"} %.2f\n", metric.name, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], metric.value);
	httpd_resp_send_chunk(req, resp, -1);

	return ESP_OK;
}

/* GET /metrics 处理函数 */
esp_err_t metrics_get_handler(httpd_req_t *req)
{
	esp_err_t ret;
	ESP_LOGI(TAG, "=========== TEMP GET HANDLER ==========");
	
	uint32_t serial_number;
	metric_sn.value = 0;
	ret = SHT3x_ReadSerialNumber(&serial_number);
	if(ret == ESP_OK) {
        ESP_LOGI(TAG, "Read SerialNumber 0k: 0x%x", serial_number);
		metric_sn.value = serial_number;
	} else {
		ESP_LOGE(TAG,"Read SerialNumber failed");
	}

    uint16_t temperature,humiture;
	metric_temperature.value = 0;
	metric_humiture.value = 0;
	ret = sht3x_get_humiture_periodic(&temperature,&humiture);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG,"temperature:%d °C, humidity:%d %%",temperature,humiture);
		metric_temperature.value = temperature;
		metric_humiture.value = humiture;
	} else {
		ESP_LOGE(TAG,"Fail to get Humiture failed");
	}

    httpd_resp_set_type(req, "text/plain");
	httpd_resp_send_prometheus_metric(req, metric_sn);
	httpd_resp_send_prometheus_metric(req, metric_temperature);
	httpd_resp_send_prometheus_metric(req, metric_humiture);

	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

httpd_uri_t metrics_handler = {
    .uri       = "/metrics",
    .method    = HTTP_GET,
    .handler   = metrics_get_handler,
    .user_ctx  = NULL
};

esp_err_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Webserver started!");
		return ret;
	}

	ESP_LOGI(TAG, "Registering URI handlers");
	httpd_register_uri_handler(server, &metrics_handler);
	return ESP_OK;
}

// Stop the httpd server
void stop_webserver(void)
{
	if (server == NULL) {
		return;
	}

	httpd_stop(server);
}
