#include "esp_http_server.h"
#include "nosedive.h"
#include "driver-pca9685.h"
#include "driver-w25q128jv.h"

struct param_handler {
	const char *prefix;
	const size_t prefix_len;
	void (*handler)(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
};

static httpd_handle_t http_tuner = NULL;
static httpd_config_t http_tuner_config = HTTPD_DEFAULT_CONFIG();

static char answer_buf[HTTP_TUNER_ANSWER_SIZE_LIMIT];
static int answer_len = 0;

extern const unsigned char panel_html_start[] asm("_binary_panel_html_gz_start");
extern const unsigned char panel_html_end[]   asm("_binary_panel_html_gz_end");

extern const unsigned char monitor_html_start[] asm("_binary_monitor_offline_html_gz_start");
extern const unsigned char monitor_html_end[]   asm("_binary_monitor_offline_html_gz_end");

static const struct param_handler handlers[] = {
	{"pcaset=",  7, &pca9685_http_handler_pcaset},
	{"pcamax=",  7, &pca9685_http_handler_pcamax},
	{"pcaoff=",  7, &pca9685_http_handler_pcaoff},
	{"pcafreq=", 8, &pca9685_http_handler_pcafreq},
	{"restart",  7, &tell_esp_to_restart},
	{"esptemp",  7, &get_temperature_info_string},
	{"espinfo",  7, &get_system_info_string},
	{"meminfo",  7, &get_w25q128jv_info_string},
};

static esp_err_t
http_tuner_parse_set(httpd_req_t *req)
{
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	const char *i = req->uri;
	while (true) {
		if (*i == '?') {
			i += 1;
			break;
		} else if (*i == '\0') {
			goto finish;
		}
		i += 1;
	}
#define URL_PARAM_SIZE 100
	char param[URL_PARAM_SIZE + 1];
	uint8_t param_len = 0;
	while (true) {
		if (*i == '&' || *i == '\0') {
			param[param_len] = '\0';
			for (size_t j = 0; j < LENGTH(handlers); ++j) {
				if (param_len >= handlers[j].prefix_len
					&& memcmp(param, handlers[j].prefix, handlers[j].prefix_len) == 0)
				{
					answer_len = 0;
					handlers[j].handler(param + handlers[j].prefix_len, answer_buf, &answer_len);
					if (answer_len > 0 && answer_len < HTTP_TUNER_ANSWER_SIZE_LIMIT) {
						httpd_resp_send_chunk(req, answer_buf, answer_len);
					}
					break;
				}
			}
			param_len = 0;
			if (*i == '\0') {
				break;
			}
		} else if (param_len < URL_PARAM_SIZE) {
			param[param_len++] = *i;
		}
		i += 1;
	}
finish:
	httpd_resp_send_chunk(req, NULL, 0); // End response.
	return ESP_OK;
}

static esp_err_t
http_tuner_panel(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	httpd_resp_send(req, (const char *)panel_html_start, panel_html_end - panel_html_start);
	return ESP_OK;
}

static esp_err_t
http_tuner_monitor(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	httpd_resp_send(req, (const char *)monitor_html_start, monitor_html_end - monitor_html_start);
	return ESP_OK;
}

static const httpd_uri_t http_tuner_set_handler = {
	.uri       = "/ctrl",
	.method    = HTTP_GET,
	.handler   = &http_tuner_parse_set,
	.user_ctx  = NULL
};

static const httpd_uri_t http_tuner_panel_handler = {
	.uri       = "/panel",
	.method    = HTTP_GET,
	.handler   = &http_tuner_panel,
	.user_ctx  = NULL
};

static const httpd_uri_t http_tuner_monitor_handler = {
	.uri       = "/monitor",
	.method    = HTTP_GET,
	.handler   = &http_tuner_monitor,
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
	httpd_register_uri_handler(http_tuner, &http_tuner_set_handler);
	httpd_register_uri_handler(http_tuner, &http_tuner_panel_handler);
	httpd_register_uri_handler(http_tuner, &http_tuner_monitor_handler);
	return true;
}

void
stop_http_tuner(void)
{
	if (httpd_stop(http_tuner) == ESP_OK) {
		http_tuner = NULL;
	}
}
