#include "esp_http_server.h"
#include <Arduino.h>
#include "main.h"

#define HTTP_TUNER_ANSWER_SIZE_LIMIT 3000

struct param_handler {
	const char *prefix;
	const size_t prefix_len;
	void (*handler)(const char *value);
	const char *args;
};

static httpd_handle_t http_tuner = NULL;
static httpd_config_t http_tuner_config = HTTPD_DEFAULT_CONFIG();
static SemaphoreHandle_t http_tuner_lock = NULL;

static char answer_buf[HTTP_TUNER_ANSWER_SIZE_LIMIT];
static int answer_len = 0;
static char *http_tuner_layout_string = NULL;

static void get_ctrl_layout_string(const char *value);

static const struct param_handler handlers[] = {
	{"get_layout",           10, &get_ctrl_layout_string,            NULL},
	{"set_engine_id",        13, &set_engine_id_command,             "1 2 3 4 5"},
	{"send_cmd",              8, &uart_send_command,                 "1,RAC,1 1,RFI,1 1,RA1,1 1,RSS,1"},
	{"set_esc_pwm",          11, &set_esc_pwm_command,               "622 626 630 634 638 642"},
	{"add_ecu_cmd",          11, &enable_ecu_telemetry_command,      "RAC RFI RA1 RSS"},
	{"delete_ecu_cmd",       14, &disable_ecu_telemetry_command,     "RAC RFI RA1 RSS"},
};

static esp_err_t
http_tuner_parse_ctrl(httpd_req_t *req)
{
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	const char *i = req->uri;
	bool in_value = false;
	while (true) {
		if (*i == '?') {
			i += 1;
			break;
		} else if (*i == '\0') {
			goto finish;
		}
		i += 1;
	}
#define URL_KEY_MAX_LENGTH   500
#define URL_VALUE_MAX_LENGTH 500
	char key[URL_KEY_MAX_LENGTH + 1], value[URL_VALUE_MAX_LENGTH + 1];
	for (size_t key_len = 0, value_len = 0; ; ++i) {
		if (*i == '&' || *i == '\0') {
			key[key_len] = '\0';
			value[value_len] = '\0';
			for (size_t j = 0; j < LENGTH(handlers); ++j) {
				if (key_len == handlers[j].prefix_len && memcmp(key, handlers[j].prefix, key_len) == 0) {
					if (xSemaphoreTake(http_tuner_lock, portMAX_DELAY) == pdTRUE) {
						answer_len = 0;
						handlers[j].handler(value);
						if (answer_len > 0 && answer_len < HTTP_TUNER_ANSWER_SIZE_LIMIT) {
							httpd_resp_send_chunk(req, answer_buf, answer_len);
						} else {
							httpd_resp_send_chunk(req, "Command was executed silently.\n", 31);
						}
						xSemaphoreGive(http_tuner_lock);
					}
					break;
				}
			}
			if (*i == '\0') break;
			key_len = 0;
			value_len = 0;
			in_value = false;
		} else if (*i == '=') {
			in_value = true;
		} else if (in_value == true) {
			if (value_len < URL_VALUE_MAX_LENGTH) value[value_len++] = *i;
		} else {
			if (key_len < URL_KEY_MAX_LENGTH) key[key_len++] = *i;
		}
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
http_tuner_start(void)
{
	http_tuner_lock = xSemaphoreCreateMutex();
	if (http_tuner_lock == NULL) return false;

	size_t http_tuner_layout_string_len = 2; // 2 curly braces
	for (size_t i = 0; i < LENGTH(handlers); ++i) {
		// 1 comma + 2 double quotes + 1 colon + 2 square brackets + 4 characters for "null" = 10
		http_tuner_layout_string_len += 10 + handlers[i].prefix_len;
		if (handlers[i].args != NULL) {
			// In the worst case, each character in args may require 2 double quotes and 1 comma, so multiply by 4.
			http_tuner_layout_string_len += 4 * strlen(handlers[i].args);
		}
	}
	http_tuner_layout_string = (char *)malloc(sizeof(char) * (http_tuner_layout_string_len + 1));
	if (http_tuner_layout_string == NULL) {
		return false;
	}
	strcpy(http_tuner_layout_string, "{");
	for (size_t i = 0; i < LENGTH(handlers); ++i) {
		if (i > 0) strcat(http_tuner_layout_string, ",");
		strcat(http_tuner_layout_string, "\"");
		strcat(http_tuner_layout_string, handlers[i].prefix);
		strcat(http_tuner_layout_string, "\":");
		if (handlers[i].args == NULL) {
			strcat(http_tuner_layout_string, "null");
		} else {
			strcat(http_tuner_layout_string, "[\"");
			for (const char *j = handlers[i].args; *j != '\0'; ++j) {
				if (*j == ' ') {
					strcat(http_tuner_layout_string, "\",\"");
				} else {
					strncat(http_tuner_layout_string, j, 1);
				}
			}
			strcat(http_tuner_layout_string, "\"]");
		}
	}
	strcat(http_tuner_layout_string, "}");

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
http_tuner_stop(void)
{
	if (httpd_stop(http_tuner) == ESP_OK) {
		http_tuner = NULL;
	}
}

void
http_tuner_response(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	answer_len = vsnprintf(answer_buf, HTTP_TUNER_ANSWER_SIZE_LIMIT, format, args);
	va_end(args);
}

static void
get_ctrl_layout_string(const char *value)
{
	http_tuner_response("%s\n", http_tuner_layout_string);
}
