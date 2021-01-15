#include <esp_log.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <sht3x.h>

static const char *TAG="WEB";

/* GET / */
esp_err_t metrics_get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "=========== TEMP GET HANDLER ==========");

    httpd_resp_set_type(req, "text/plain");
	
	uint32_t serial_number;
    if(SHT3x_ReadSerialNumber(&serial_number) == 0) {
        ESP_LOGI(TAG, "Read SerialNumber 0k: 0x%x \n", serial_number);
	} else {
		ESP_LOGE(TAG,"Read SerialNumber failed \n");
	}

    uint16_t temperature,humiture;
	if(sht3x_get_humiture_periodic(&temperature,&humiture) == 0) {
		ESP_LOGI(TAG,"temperature:%d °C, humidity:%d %%\n",temperature,humiture);
	} else {
		ESP_LOGE(TAG,"Get_Humiture failed\n");
	}

	
    char resp[256];
	httpd_resp_send_chunk(req, "# HELP temperature_celsius temperature as celsius\n", -1);
	httpd_resp_send_chunk(req, "# TYPE temperature_celsius gauge\n", -1);
	sprintf(resp, "temperature_celsius %d\n",temperature);
	httpd_resp_send_chunk(req, resp, -1);

	memset(resp, 0, sizeof(resp));
	httpd_resp_send_chunk(req, "# HELP humiture Humiture\n", -1);
	httpd_resp_send_chunk(req, "# TYPE humiture gauge\n", -1);
	sprintf(resp, "humiture %d\n",humiture);
	httpd_resp_send_chunk(req, resp, -1);

	memset(resp, 0, sizeof(resp));
	httpd_resp_send_chunk(req, "# HELP serial_number Serial number of SHT3x\n", -1);
	httpd_resp_send_chunk(req, "# TYPE serial_number gauge\n", -1);
	sprintf(resp, "serial_number %d\n",serial_number);
	httpd_resp_send_chunk(req, resp, -1);

	httpd_resp_send_chunk(req, NULL, 0);

	return ESP_OK;
}

httpd_uri_t metrics_handler = {
    .uri       = "/metrics",
    .method    = HTTP_GET,
    .handler   = metrics_get_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &metrics_handler);
        return server;
    }

    ESP_LOGI(TAG, "Webserver started!");
    return NULL;
}

// Stop the httpd server
void stop_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}
