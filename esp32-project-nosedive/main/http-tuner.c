#include "esp_http_server.h"
#include "nosedive.h"

struct param_handler {
	const char *prefix;
	const size_t prefix_len;
	void (*handler)(const char *value);
};

static httpd_handle_t http_tuner = NULL;
static httpd_config_t http_tuner_config = HTTPD_DEFAULT_CONFIG();

extern const unsigned char panel_html_start[] asm("_binary_panel_html_start");
extern const unsigned char panel_html_end[]   asm("_binary_panel_html_end");

extern const unsigned char monitor_html_start[] asm("_binary_monitor_offline_html_gz_start");
extern const unsigned char monitor_html_end[]   asm("_binary_monitor_offline_html_gz_end");

static const struct param_handler handlers[] = {
	{"pcaset=",  7, &pca9685_http_handler_pcaset},
	{"pcamax=",  7, &pca9685_http_handler_pcamax},
	{"pcaoff=",  7, &pca9685_http_handler_pcaoff},
	{"pcafreq=", 8, &pca9685_http_handler_pcafreq},
	{"restart",  7, &tell_esp_to_restart},
};

static esp_err_t
http_tuner_parse_set(httpd_req_t *req)
{
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
			param_len = 0;
			for (size_t j = 0; j < LENGTH(handlers); ++j) {
				if (strncmp(param, handlers[j].prefix, handlers[j].prefix_len) == 0) {
					handlers[j].handler(param + handlers[j].prefix_len);
					break;
				}
			}
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
	httpd_resp_send(req, (const char *)panel_html_start, panel_html_end - panel_html_start);
	httpd_resp_send_chunk(req, NULL, 0); // End response.
	return ESP_OK;
}

static esp_err_t
http_tuner_monitor(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	httpd_resp_send(req, (const char *)monitor_html_start, monitor_html_end - monitor_html_start);
	httpd_resp_send_chunk(req, NULL, 0); // End response.
	return ESP_OK;
}

static const httpd_uri_t http_tuner_set_handler = {
	.uri       = "/set",
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
