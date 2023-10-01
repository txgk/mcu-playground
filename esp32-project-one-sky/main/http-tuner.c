#include "esp_http_server.h"
#include "main.h"

struct param_handler {
	const char *prefix;
	const size_t prefix_len;
	void (*handler)(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
	const char *args;
};

static httpd_handle_t http_tuner = NULL;
static httpd_config_t http_tuner_config = HTTPD_DEFAULT_CONFIG();

static char answer_buf[HTTP_TUNER_ANSWER_SIZE_LIMIT];
static int answer_len = 0;

void get_ctrl_layout_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

static const struct param_handler handlers[] = {
	{"restart",                    7, &tell_esp_to_restart,                         NULL},
	{"esptemp",                    7, &get_temperature_info_string,                 NULL},
	{"espinfo",                    7, &get_system_info_string,                      NULL},

	{"set_throttle",              12, &driver_amt_engine_set_throttle,              "0 100 200 300 400 500 600 700 800 900 1000"},

	{"control_drop",              12, &driver_amt_engine_control_drop,              NULL},
	{"control_off",               11, &driver_amt_engine_control_off,               NULL},
	{"control_ready",             13, &driver_amt_engine_control_ready,             NULL},
	{"control_start",             13, &driver_amt_engine_control_start,             NULL},

	{"exhaust_fuel",              12, &driver_amt_engine_exhaust_fuel,              NULL},
	{"test_glowplug",             13, &driver_amt_engine_test_glowplug,             NULL},
	{"test_main_valve",           15, &driver_amt_engine_test_main_valve,           NULL},
	{"test_ignition_valve",       19, &driver_amt_engine_test_ignition_valve,       NULL},
	{"test_pump",                  9, &driver_amt_engine_test_pump,                 NULL},
	{"test_starter",              12, &driver_amt_engine_test_starter,              NULL},
	{"set_update_rate_20hz",      20, &driver_amt_engine_set_update_rate_20hz,      NULL},
	{"set_update_rate_50hz",      20, &driver_amt_engine_set_update_rate_50hz,      NULL},
	{"set_update_rate_100hz",     21, &driver_amt_engine_set_update_rate_100hz,     NULL},
	{"reset_fuel_flow",           15, &driver_amt_engine_reset_fuel_flow,           NULL},
	{"calibrate_thrust_zero",     21, &driver_amt_engine_calibrate_thrust_zero,     NULL},

	{"set_ignition_pump_voltage", 25, &driver_amt_engine_set_ignition_pump_voltage, "1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0 5.5 6.0"},
	{"set_acceleration_curve",    22, &driver_amt_engine_set_acceleration_curve,    "0 1 2 3 4 5 6 7 8 9 10"},

	{"get_layout",                10, &get_ctrl_layout_string,                      NULL},
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
					answer_len = 0;
					handlers[j].handler(value, answer_buf, &answer_len);
					if (answer_len > 0 && answer_len < HTTP_TUNER_ANSWER_SIZE_LIMIT) {
						httpd_resp_send_chunk(req, answer_buf, answer_len);
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

void
get_ctrl_layout_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	strcpy(answer_buf_ptr, "{");
	for (size_t i = 0; i < LENGTH(handlers); ++i) {
		if (i > 0) strcat(answer_buf_ptr, ",");
		strcat(answer_buf_ptr, "\"");
		strcat(answer_buf_ptr, handlers[i].prefix);
		strcat(answer_buf_ptr, "\":");
		if (handlers[i].args == NULL) {
			strcat(answer_buf_ptr, "null");
		} else {
			strcat(answer_buf_ptr, "[\"");
			for (const char *j = handlers[i].args; *j != '\0'; ++j) {
				if (*j == ' ') {
					strcat(answer_buf_ptr, "\",\"");
				} else {
					strncat(answer_buf_ptr, j, 1);
				}
			}
			strcat(answer_buf_ptr, "\"]");
		}
	}
	strcat(answer_buf_ptr, "}\n");
	*answer_len_ptr = strlen(answer_buf_ptr);
}
