#include "esp_http_server.h"
#include "main.h"

#define WEBSOCKET_STREAMER_MAX_MESSAGE_SIZE 1000

static httpd_handle_t http_streamer = NULL;
static httpd_config_t http_streamer_config = HTTPD_DEFAULT_CONFIG();
static volatile bool websocket_streamer_is_connected = false;
static SemaphoreHandle_t websocket_streamer_lock = NULL;
static char websocket_streamer_data_buf[WEBSOCKET_STREAMER_MAX_MESSAGE_SIZE];
static volatile size_t websocket_streamer_data_len = 0;

static const char *names_string[] = {
	"UART_AMT_1@names=Rpm(rpm),EngineState(bitmask),ErrorCode(bitmask),EngineTemp(C),SwitchState(bitmask)\n",
	"UART_AMT_2@names=Rpm(rpm),RadioVoltage(0.1V),PowerVoltage(0.1V),PumpVoltage(0.1V)\n",
	"UART_AMT_3@names=Rpm(rpm),Throttle(%),Pressure(Pa)\n",
	"UART_AMT_4@names=Rpm(rpm),Current(0.1A),Thrust(0.1kg)\n",
	"UART_AMT_5@names=Rpm(rpm),IgnitionPumpVoltage(0.01V),CurveIncrease(?),CurveDecrease(?)\n",
	"UART_AMT_6@names=Rpm(rpm),MaxRpm(rpm),MaxPumpVoltage(0.1V),ProtocolVersion(bitmask),SendRate(bitmask)\n",
	"UART_AMT_7@names=Rpm(rpm),FlowRate(0.01l/min),FlowTotal(0.1l)\n",
	"UART_AMT_8@names=Rpm(rpm),IdleRpm(rpm)\n"
};

static esp_err_t
http_streamer_handler(httpd_req_t *req)
{
	websocket_streamer_is_connected = true;
	for (size_t i = 0; i < LENGTH(names_string); ++i) {
		httpd_ws_frame_t names_pkt = {
			.final = true,
			.fragmented = false,
			.type = HTTPD_WS_TYPE_TEXT,
			.payload = (uint8_t *)names_string[i],
			.len = strlen(names_string[i]),
		};
		if (httpd_ws_send_frame(req, &names_pkt) != ESP_OK) {
			websocket_streamer_is_connected = false;
			return ESP_FAIL;
		}
	}
	while (true) {
		if (websocket_streamer_data_len > 0) {
			httpd_ws_frame_t ws_pkt = {
				.final = true,
				.fragmented = false,
				.type = HTTPD_WS_TYPE_TEXT,
				.payload = (uint8_t *)websocket_streamer_data_buf,
				.len = websocket_streamer_data_len,
			};
			if (httpd_ws_send_frame(req, &ws_pkt) != ESP_OK) {
				websocket_streamer_is_connected = false;
				return ESP_FAIL;
			}
			websocket_streamer_data_len = 0;
		} else {
			TASK_DELAY_MS(50);
		}
	}
	websocket_streamer_is_connected = false;
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
	websocket_streamer_lock = xSemaphoreCreateMutex();
	if (websocket_streamer_lock == NULL) {
		return false;
	}
	http_streamer_config.server_port = HTTP_STREAMER_PORT;
	http_streamer_config.ctrl_port = HTTP_STREAMER_CTRL;
	http_streamer_config.lru_purge_enable = true;
	if (httpd_start(&http_streamer, &http_streamer_config) != ESP_OK) {
		return false;
	}
	httpd_register_uri_handler(http_streamer, &http_streamer_handler_setup);
	return true;
}

void
write_websocket_message(const char *buf, size_t len)
{
	if (websocket_streamer_is_connected == true && buf != NULL && len != 0) {
		if (xSemaphoreTake(websocket_streamer_lock, portMAX_DELAY) == pdTRUE) {
			for (uint8_t i = 0; i < 20; ++i) {
				if (websocket_streamer_data_len == 0) {
					websocket_streamer_data_len = len > WEBSOCKET_STREAMER_MAX_MESSAGE_SIZE ? WEBSOCKET_STREAMER_MAX_MESSAGE_SIZE : len;
					memcpy(websocket_streamer_data_buf, buf, websocket_streamer_data_len);
					break;
				}
				TASK_DELAY_MS(50);
			}
			xSemaphoreGive(websocket_streamer_lock);
		}
	}
}

int
write_websocket_message_vprintf(const char *fmt, va_list args)
{
	char out[1000];
	int wrote = vsnprintf(out, 1000, fmt, args);
	if (wrote > 0 && wrote < 1000) {
		write_websocket_message(out, wrote);
	}
	return wrote;
}
