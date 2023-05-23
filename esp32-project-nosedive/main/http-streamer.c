#include "esp_http_server.h"
#include "nosedive.h"

static httpd_handle_t http_streamer = NULL;
static httpd_config_t http_streamer_config = HTTPD_DEFAULT_CONFIG();

static volatile bool http_streamer_is_streaming = false;

static const char *data_buf = NULL;
static volatile size_t data_len = 0;
static volatile bool data_has_been_sent = true;

static void
http_streamer_active(void)
{
	if (xSemaphoreTake(system_mutexes[MUX_HTTP_STREAMER_ACTIVITY_INDICATOR], portMAX_DELAY) == pdTRUE) {
		http_streamer_is_streaming = true;
		xSemaphoreGive(system_mutexes[MUX_HTTP_STREAMER_ACTIVITY_INDICATOR]);
	}
}

static void
http_streamer_inactive(void)
{
	if (xSemaphoreTake(system_mutexes[MUX_HTTP_STREAMER_ACTIVITY_INDICATOR], portMAX_DELAY) == pdTRUE) {
		http_streamer_is_streaming = false;
		xSemaphoreGive(system_mutexes[MUX_HTTP_STREAMER_ACTIVITY_INDICATOR]);
	}
}

static bool
get_http_streamer_activity_status(void)
{
	bool status = false;
	if (xSemaphoreTake(system_mutexes[MUX_HTTP_STREAMER_ACTIVITY_INDICATOR], portMAX_DELAY) == pdTRUE) {
		status = http_streamer_is_streaming;
		xSemaphoreGive(system_mutexes[MUX_HTTP_STREAMER_ACTIVITY_INDICATOR]);
	}
	return status;
}

static esp_err_t
http_streamer_all_handler(httpd_req_t *req)
{
	http_streamer_active();
	while (true) { // TODO: check if it okay to make iniftine loop in handler
		if (data_has_been_sent == false) {
			if (httpd_resp_send_chunk(req, data_buf, data_len) != ESP_OK) {
				break;
			}
			data_has_been_sent = true;
		}
		TASK_DELAY_MS(10);
	}
	http_streamer_inactive();
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
send_data(const char *new_data_buf, size_t new_data_len)
{
	if (http_streamer == NULL || get_http_streamer_activity_status() == false) {
		return;
	}
	if (xSemaphoreTake(system_mutexes[MUX_HTTP_STREAMER], portMAX_DELAY) == pdTRUE) {
		data_buf = new_data_buf;
		data_len = new_data_len;
		data_has_been_sent = false;
		while (data_has_been_sent == false) {
			TASK_DELAY_MS(10);
		}
		xSemaphoreGive(system_mutexes[MUX_HTTP_STREAMER]);
	}
}

void
stop_http_streamer(void)
{
	if (httpd_stop(http_streamer) == ESP_OK) {
		http_streamer = NULL;
	}
}
