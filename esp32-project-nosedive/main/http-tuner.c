#include "esp_http_server.h"
#include "nosedive.h"

static httpd_handle_t http_tuner = NULL;
static httpd_config_t http_tuner_config = HTTPD_DEFAULT_CONFIG();

static esp_err_t
http_tuner_heart_beat_handler(httpd_req_t *req)
{
	uart_write_bytes(UART_NUM_0, req->uri, strlen(req->uri));
	uart_write_bytes(UART_NUM_0, "\n", 1);
	httpd_resp_send_chunk(req, NULL, 0); // End response.
	return ESP_OK;
}

static const httpd_uri_t http_tuner_heart_beat_handler_setup = {
	.uri       = "/beat",
	.method    = HTTP_GET,
	.handler   = &http_tuner_heart_beat_handler,
	.user_ctx  = NULL
};

bool
start_http_tuner(void)
{
	http_tuner_config.server_port = HTTP_TUNER_PORT;
	http_tuner_config.ctrl_port = HTTP_TUNER_CTRL;
	http_tuner_config.lru_purge_enable = true;
	if (httpd_start(&http_tuner, &http_tuner_config) != ESP_OK) {
		return false;
	}
	httpd_register_uri_handler(http_tuner, &http_tuner_heart_beat_handler_setup);
	return true;
}

void
stop_http_tuner(void)
{
	if (httpd_stop(http_tuner) == ESP_OK) {
		http_tuner = NULL;
	}
}
