#include "esp_http_server.h"
#include "interwind.h"

static httpd_handle_t http_streamer = NULL;
static httpd_config_t http_streamer_config = HTTPD_DEFAULT_CONFIG();

static esp_err_t
http_streamer_handler(httpd_req_t *req)
{
	while (true) {
		for (size_t i = 0; tasks[i].prefix != NULL; ++i) {
			int64_t current_time_ms = esp_timer_get_time() / 1000;
			if (tasks[i].last_inform_ms[0] + tasks[i].informer_period_ms < current_time_ms) {
				tasks[i].last_inform_ms[0] = current_time_ms;
				int data_len = tasks[i].informer(&tasks[i], tasks[i].informer_data[0]);
				if (data_len > 0) {
					httpd_ws_frame_t ws_pkt = {
						.final = true,
						.fragmented = false,
						.type = HTTPD_WS_TYPE_TEXT,
						.payload = (uint8_t *)tasks[i].informer_data[0],
						.len = data_len,
					};
					if (httpd_ws_send_frame(req, &ws_pkt) != ESP_OK) {
						return ESP_FAIL; // Вот и всё, до свидания, чёрт с тобой...
					}
				}
			}
		}
		TASK_DELAY_MS(100);
	}
	return ESP_OK;
}

static const httpd_uri_t http_streamer_handler_setup = {
	.uri       = "/",
	.method    = HTTP_GET,
	.handler   = &http_streamer_handler,
	.user_ctx  = NULL,
	.is_websocket = true
};

bool
start_websocket_streamer(void)
{
	http_streamer_config.server_port = HTTP_STREAMER_PORT;
	http_streamer_config.ctrl_port = HTTP_STREAMER_CTRL;
	http_streamer_config.lru_purge_enable = true;
	if (httpd_start(&http_streamer, &http_streamer_config) != ESP_OK) {
		return false;
	}
	httpd_register_uri_handler(http_streamer, &http_streamer_handler_setup);
	return true;
}
