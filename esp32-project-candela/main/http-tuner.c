#include "esp_http_server.h"
#include "candela.h"

struct param_handler {
	const char *prefix;
	const size_t prefix_len;
	void (*handler)(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
};

static httpd_handle_t http_tuner = NULL;
static httpd_config_t http_tuner_config = HTTPD_DEFAULT_CONFIG();

static char answer_buf[HTTP_TUNER_ANSWER_SIZE_LIMIT];
static int answer_len = 0;

static const struct param_handler handlers[] = {
	{"restart",        7, &tell_esp_to_restart},
	{"esptemp",        7, &get_temperature_info_string},
	{"espinfo",        7, &get_system_info_string},
	{"engine_on",      9, &can_jetcat_engine_on},
	{"engine_off",    10, &can_jetcat_engine_off},
	{"engine_thrust", 13, &can_jetcat_engine_thrust},
	{"get_layout",    10, &get_ctrl_layout_string},
};

static esp_err_t
http_tuner_parse_ctrl(httpd_req_t *req)
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
					if (param[handlers[j].prefix_len] == '=') {
						handlers[j].handler(param + handlers[j].prefix_len + 1, answer_buf, &answer_len);
					} else {
						handlers[j].handler(param + handlers[j].prefix_len, answer_buf, &answer_len);
					}
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

static const httpd_uri_t http_tuner_ctrl_handler = {
	.uri       = "/ctrl",
	.method    = HTTP_GET,
	.handler   = &http_tuner_parse_ctrl,
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
	httpd_register_uri_handler(http_tuner, &http_tuner_ctrl_handler);
	return true;
}

void
stop_http_tuner(void)
{
	if (httpd_stop(http_tuner) == ESP_OK) {
		http_tuner = NULL;
	}
}
