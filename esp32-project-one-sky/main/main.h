#ifndef MAIN_HEADER_GUARD_H
#define MAIN_HEADER_GUARD_H
#include <stdatomic.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/twai.h"
#include "esp_timer.h"
#include "esp_log.h"
#define USE_WIFI_STATION
#ifdef USE_WIFI_STATION
#include "../../wifi-credentials.h"
#endif

#define FIRMWARE_CODEWORD  "Щелкунчик"

#define UART0_TX_PIN        1
#define UART0_RX_PIN        3
#define UART0_SPEED         115200
#define UART0_PORT          UART_NUM_0

#define AMT_UART_SPEED      9600
#define AMT_UART_TX_PIN     16
#define AMT_UART_RX_PIN     18
#define AMT_UART_PORT       UART_NUM_1

#define HX711_SAMPLES_COUNT 10
#define HX711_DATA_PIN      14
#define HX711_SCLK_PIN      12
#define HX711_MEASURE_GAIN   1 // 1 = 128, 2 = 32, 3 = 64

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

bool start_serial_streamer(void);                          // См. файл "streamer-serial.c"
bool start_tcp_streamer(void);                             // См. файл "streamer-tcp.c"
void write_tcp_message(const char *buf, size_t len);       // См. файл "streamer-tcp.c"
bool start_websocket_streamer(void);                       // См. файл "streamer-websocket.c"
void write_websocket_message(const char *buf, size_t len); // См. файл "streamer-websocket.c"

bool start_http_tuner(void); // См. файл "http-tuner.c"
void stop_http_tuner(void);  // См. файл "http-tuner.c"

// См. файл "common-i2c.c"
uint8_t i2c_read_one_byte_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);
uint16_t i2c_read_two_bytes_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);

// См. файл "system-info.c"
void get_system_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void get_temperature_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void get_ctrl_layout_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

// См. файл "driver-amt.c"
bool driver_amt_init(void);
void driver_amt_engine_control_off(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_control_ready(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_control_start(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

void driver_amt_engine_exhaust_fuel(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_test_glowplug(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_test_main_valve(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_test_ignition_valve(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_test_pump(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_test_starter(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_set_update_rate_20hz(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_set_update_rate_50hz(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_set_update_rate_100hz(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_reset_fuel_flow(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void driver_amt_engine_calibrate_thrust_zero(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

// См. файл "driver-hx711.c"
bool hx711_init(void);

extern struct task_descriptor tasks[];
#endif // MAIN_HEADER_GUARD_H
