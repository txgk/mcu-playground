#ifndef CANDELA_H
#define CANDELA_H
#include <stdatomic.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/twai.h"
#include "esp_timer.h"
#include "esp_log.h"
#define CANDELA_USE_WIFI_STATION
#ifdef CANDELA_USE_WIFI_STATION
#include "../../wifi-credentials.h"
#endif

#define FIRMWARE_CODEWORD  "Щелкунчик"

#define WIFI_AP_SSID                "demoproshivka"
#define WIFI_AP_PASS                "elbereth"
#define WIFI_RECONNECTION_PERIOD_MS 2000

#define HTTP_STREAMER_PORT 222
#define HTTP_TUNER_PORT    333
#define HTTP_STREAMER_CTRL 2222
#define HTTP_TUNER_CTRL    3333

#define LENGTH(A) (sizeof((A))/sizeof(*(A)))
#define MS_TO_TICKS(A) ((A) / portTICK_PERIOD_MS)
#define CYCLES_TO_US(A) ((A) / cpu_frequency_mhz)
#define US_TO_CYCLES(A) ((A) * cpu_frequency_mhz)
#define TASK_DELAY_MS(A) vTaskDelay((A) / portTICK_PERIOD_MS)
#define INIT_IP4_LOL(a, b, c, d) { .type = ESP_IPADDR_TYPE_V4, .u_addr = { .ip4 = { .addr = ESP_IP4TOADDR(a, b, c, d) }}}

#define MESSAGE_SIZE_LIMIT 1000
#define HTTP_TUNER_ANSWER_SIZE_LIMIT 1000

struct task_descriptor {
	const char *prefix;
	void (*performer)(void *);
	int (*informer)(struct task_descriptor *, char *); // Returns the number of bytes in respective informer_data
	const int64_t performer_period_ms;
	const int64_t informer_period_ms;
	const uint32_t stack_size;
	const unsigned int priority;
	// last_inform_ms[0] and informer_data[MESSAGE_SIZE_LIMIT][0] are for WebSocket streamer
	// last_inform_ms[1] and informer_data[MESSAGE_SIZE_LIMIT][1] are for Serial streamer
	// last_inform_ms[2] and informer_data[MESSAGE_SIZE_LIMIT][2] are for Winbond streamer
	int64_t last_inform_ms[3];
	char informer_data[MESSAGE_SIZE_LIMIT][3];
	SemaphoreHandle_t mutex;
};

void tell_esp_to_restart(const char *value, char *answer_buf_ptr, int *answer_len_ptr); // См. файл "main.c"

void start_tasks(void); // См. файл "tasks.c"

bool start_tcp_streamer(void);                             // См. файл "streamer-tcp.c"
void write_tcp_message(const char *buf, size_t len);       // См. файл "streamer-tcp.c"
bool start_websocket_streamer(void);                       // См. файл "streamer-websocket.c"
void write_websocket_message(const char *buf, size_t len); // См. файл "streamer-websocket.c"
int write_websocket_message_vprintf(const char *fmt, va_list args);

bool start_http_tuner(void); // См. файл "http-tuner.c"
void stop_http_tuner(void);  // См. файл "http-tuner.c"

// См. файл "common-i2c.c"
uint8_t i2c_read_one_byte_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);
uint16_t i2c_read_two_bytes_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);

// См. файл "system-info.c"
void get_system_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void get_temperature_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void get_ctrl_layout_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

extern struct task_descriptor tasks[];
#endif // CANDELA_H
