#include "esp_http_server.h"
#include "nosedive.h"

static httpd_handle_t http_streamer = NULL;
static httpd_config_t http_streamer_config = HTTPD_DEFAULT_CONFIG();

static esp_err_t
http_streamer_all_handler(httpd_req_t *req)
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

static const httpd_uri_t http_streamer_all_handler_setup = {
	.uri       = "/all",
	.method    = HTTP_GET,
	.handler   = &http_streamer_all_handler,
	.user_ctx  = NULL
};

bool
start_http_streamer(void)
{
	http_streamer_config.server_port = HTTP_STREAMER_PORT;
	http_streamer_config.ctrl_port = HTTP_STREAMER_CTRL;
	http_streamer_config.lru_purge_enable = true;
	if (httpd_start(&http_streamer, &http_streamer_config) != ESP_OK) {
		return false;
	}
	httpd_register_uri_handler(http_streamer, &http_streamer_all_handler_setup);
	return true;
}

void
stop_http_streamer(void)
{
	if (httpd_stop(http_streamer) == ESP_OK) {
		http_streamer = NULL;
	}
}
