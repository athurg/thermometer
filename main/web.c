#include <esp_log.h>
#include <nvs.h>
#include <esp_http_server.h>

httpd_handle_t server = NULL;
const char *TAG = "main.web";

const char *html = "<html>"
"<head>"
"<title>Param config for ESP</title>"
"<meta name='viewport' content='width=device-width'>"
"</head>"
"<body>"
"<h1>ESP Param config</h1>"
"<form>"
"WiFi SSID: <input name='wifi_ssid'/><br/>"
"WiFi Password: <input name='wifi_pass'/><br/>"
"MQTT Addr: <input name='mqtt_host'/><br/>"
"<input type='submit'/>"
"</form>"
"</body></html>";

static void handle_querystring(const char *querystring)
{
	esp_err_t ret;
	char wifi_ssid[32];
	ret = httpd_query_key_value(querystring, "wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Fail to get wifi_ssid param from QueryString %d", ret);
		return;
	}

	char wifi_pass[32];
	ret = httpd_query_key_value(querystring, "wifi_pass", wifi_pass, sizeof(wifi_pass));
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Fail to get wifi_pass param from QueryString %d", ret);
		return;
	}

	char mqtt_addr[100];
	ret = httpd_query_key_value(querystring, "mqtt_addr", mqtt_addr, sizeof(mqtt_addr));
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Fail to get mqtt_addr param from QueryString %d", ret);
		return;
	}

	nvs_handle nvs_handle;
	ret = nvs_open("params", NVS_READWRITE, &nvs_handle);
	if (ret!=ESP_OK) {
		ESP_LOGI(TAG, "Fail to open NVS param handle %d", ret);
		return;
	}

	ret = nvs_set_str(nvs_handle, "wifi_ssid", wifi_ssid);
	if (ret != ESP_OK) {
		nvs_close(nvs_handle);
		ESP_LOGE(TAG, "Fail to write wifi_ssid param to NVS %d", ret);
		return;
	}

	ret = nvs_set_str(nvs_handle, "wifi_pass", wifi_pass);
	if (ret != ESP_OK) {
		nvs_close(nvs_handle);
		ESP_LOGE(TAG, "Fail to write wifi_pass param to NVS %d", ret);
		return;
	}

	ret = nvs_set_str(nvs_handle, "mqtt_addr", mqtt_addr);
	if (ret != ESP_OK) {
		nvs_close(nvs_handle);
		ESP_LOGE(TAG, "Fail to write mqtt_addr param to NVS %d", ret);
		return;
	}

	nvs_close(nvs_handle);

	ESP_LOGI(TAG, "Param saved wifi_ssid=%s wifi_pass=%s mqtt_addr=%s", wifi_ssid, wifi_pass, mqtt_addr);
}


esp_err_t index_handler(httpd_req_t *req)
{
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1) {
		char *buf = malloc(buf_len);
		esp_err_t ret = httpd_req_get_url_query_str(req, buf, buf_len);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Fail to get url query string (%d)", ret);
		} else {
			ESP_LOGI(TAG, "QueryString => %s", buf);
			handle_querystring(buf);
		}
		free(buf);
	}

    httpd_resp_send_chunk(req, html, -1);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t start_webserver(void)
{
	esp_err_t ret;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
		return ret;
	}

	httpd_uri_t index = {
		.uri       = "/",
		.method    = HTTP_GET,
		.handler   = index_handler,
		.user_ctx  = NULL
	};

	// Set URI handlers
	ESP_LOGI(TAG, "Registering URI handlers");
	httpd_register_uri_handler(server, &index);

    return ESP_OK;
}

