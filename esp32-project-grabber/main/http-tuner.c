#include "esp_http_server.h"
#include "main.h"

#define HTTP_TUNER_ANSWER_SIZE_LIMIT 2000

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
	{"restart",               7, &tell_esp_to_restart,               NULL},
	{"esptemp",               7, &get_temperature_info_string,       NULL},
	{"espinfo",               7, &get_system_info_string,            NULL},

	{"servo_setup",          11, &nt60_servo_setup,                  "1 2 3 4 5 6 7 8"},
	{"seek_extremes",        13, &nt60_seek_extremes,                "1 2 3 4 5 6 7 8"},
	{"rotate_absolute",      15, &nt60_rotate_absolute,              "1,0 1,100 1,250 1,500 1,1000 1,2000 1,3000 1,4000 1,8000 1,16000 1,32000"},
	{"rotate_relative",      15, &nt60_rotate_relative,              "1,100 1,-100 1,250 1,-250 1,500 1,-500 1,1000 1,-1000 "
	                                                                 "2,100 2,-100 2,250 2,-250 2,500 2,-500 2,1000 2,-1000 "
	                                                                 "3,100 3,-100 3,250 3,-250 3,500 3,-500 3,1000 3,-1000"},
	{"rotate_stop",          11, &nt60_servo_stop,                   "1 2 3 4 5 6 7 8"},
	{"set_speed",             9, &nt60_set_speed,                    "1,100 1,200 1,300 1,400 1,500 1,600"},
	{"get_speed",             9, &nt60_get_speed,                    "1 2 3 4 5 6 7 8"},
	{"set_acceleration",     16, &nt60_set_acceleration,             "1,100 1,200 1,300 1,400 1,500 1,600"},
	{"get_acceleration",     16, &nt60_get_acceleration,             "1 2 3 4 5 6 7 8"},
	{"set_deceleration",     16, &nt60_set_deceleration,             "1,100 1,200 1,300 1,400 1,500 1,600"},
	{"get_deceleration",     16, &nt60_get_deceleration,             "1 2 3 4 5 6 7 8"},
	{"set_current",          11, &nt60_set_current,                  "1,1000 1,2000 1,3000 1,4000 1,5000"},
	{"get_current",          11, &nt60_get_current,                  "1 2 3 4 5 6 7 8"},
	{"set_track_err_thres",  19, &nt60_set_tracking_error_threshold, "1,100 1,500 1,1000 1,2000 1,4000"},
	{"get_track_err_thres",  19, &nt60_get_tracking_error_threshold, "1 2 3 4 5 6 7 8"},
	{"save_config_to_flash", 20, &nt60_save_config_to_flash,         "1,1 2,1 3,1 4,1 5,1 6,1 7,1 8,1"},
	{"read_short_reg",       14, &nt60_read_short_register,          "1,2 1,3 1,25 1,29 1,41 1,45 1,46"},
	{"read_long_reg",        13, &nt60_read_long_register,           "1,8 1,12"},

	{"stream_websocket_on",  19, &streamer_websocket_enable,         NULL},
	{"stream_websocket_off", 20, &streamer_websocket_disable,        NULL},
	{"stream_tcp_on",        13, &streamer_tcp_enable,               NULL},
	{"stream_tcp_off",       14, &streamer_tcp_disable,              NULL},

	{"get_layout",           10, &get_ctrl_layout_string,            NULL},
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
#define URL_KEY_MAX_LENGTH   50
#define URL_VALUE_MAX_LENGTH 50
	char key[URL_KEY_MAX_LENGTH + 1], value[URL_VALUE_MAX_LENGTH + 1];
	bool in_value = false;
	for (uint8_t key_len = 0, value_len = 0; ; ++i) {
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
	http_tuner_layout_string = malloc(sizeof(char) * (http_tuner_layout_string_len + 1));
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
