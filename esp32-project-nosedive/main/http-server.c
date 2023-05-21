#include "esp_http_server.h"
#include "nosedive.h"

static httpd_handle_t http_server = NULL;
static httpd_config_t http_server_config = HTTPD_DEFAULT_CONFIG();

static esp_err_t
http_server_ping_handler(httpd_req_t *req)
{
	httpd_resp_send(req, "pong\n", HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

static esp_err_t
http_server_data_handler(httpd_req_t *req)
{
	while (true) {
		if (httpd_resp_send_chunk(req, "very important data\n", 20) != ESP_OK) {
			break;
		}
		TASK_DELAY_MS(500);
	}
	httpd_resp_send_chunk(req, NULL, 0); // End response.
	return ESP_OK;
}

static const httpd_uri_t http_stream_ping_handler_setup = {
	.uri       = "/ping",
	.method    = HTTP_GET,
	.handler   = &http_server_ping_handler,
	.user_ctx  = NULL
};

static const httpd_uri_t http_stream_data_handler_setup = {
	.uri       = "/data",
	.method    = HTTP_GET,
	.handler   = &http_server_data_handler,
	.user_ctx  = NULL
};

bool
start_http_server(void)
{
	http_server_config.server_port = HTTP_SERVER_PORT;
	http_server_config.lru_purge_enable = true;
	if (httpd_start(&http_server, &http_server_config) != ESP_OK) {
		return false;
	}
	httpd_register_uri_handler(http_server, &http_stream_ping_handler_setup);
	httpd_register_uri_handler(http_server, &http_stream_data_handler_setup);
	return true;
}

bool
stop_http_server(void)
{
	return true;
}
