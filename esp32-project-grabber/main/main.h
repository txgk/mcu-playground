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

#define FIRMWARE_CODEWORD  "Хвататель"
#define ENABLE_HEARTBEAT   1

#define UART0_TX_PIN        1
#define UART0_RX_PIN        3
#define UART0_SPEED         115200
#define UART0_PORT          UART_NUM_1

#define WIFI_AP_SSID                "demoproshivka"
#define WIFI_AP_PASS                "elbereth"
#define WIFI_RECONNECTION_PERIOD_MS 2000

#define TCP_STREAMER_PORT 111
#define WS_STREAMER_PORT  222
#define HTTP_TUNER_PORT   333
#define WS_STREAMER_CTRL  2222
#define HTTP_TUNER_CTRL   3333

#define LENGTH(A) (sizeof((A))/sizeof(*(A)))
#define MS_TO_TICKS(A) ((A) / portTICK_PERIOD_MS)
#define CYCLES_TO_US(A) ((A) / cpu_frequency_mhz)
#define US_TO_CYCLES(A) ((A) * cpu_frequency_mhz)
#define TASK_DELAY_MS(A) vTaskDelay((A) / portTICK_PERIOD_MS)
#define INIT_IP4_LOL(a, b, c, d) { .type = ESP_IPADDR_TYPE_V4, .u_addr = { .ip4 = { .addr = ESP_IP4TOADDR(a, b, c, d) }}}

#define TCP_STREAMER_MAX_PACKET_SIZE        1000
#define WEBSOCKET_STREAMER_MAX_MESSAGE_SIZE 1000
#define HTTP_TUNER_ANSWER_SIZE_LIMIT        1000

#ifdef CONFIG_IDF_TARGET_ESP32
uint8_t temprature_sens_read(); // Undocumented secret function!
#endif

void tell_esp_to_restart(const char *value, char *answer_buf_ptr, int *answer_len_ptr); // См. файл "main.c"

// См. файл "streamer.c"
void stream_write(const char *buf, size_t len);
void stream_panic(const char *buf, size_t len);
void streamer_websocket_enable(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void streamer_websocket_disable(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void streamer_tcp_enable(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void streamer_tcp_disable(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

bool start_tcp_streamer(void);                             // См. файл "streamer-tcp.c"
void write_tcp_message(const char *buf, size_t len);       // См. файл "streamer-tcp.c"
bool start_websocket_streamer(void);                       // См. файл "streamer-websocket.c"
void write_websocket_message(const char *buf, size_t len); // См. файл "streamer-websocket.c"
int write_websocket_message_vprintf(const char *fmt, va_list args);

bool start_http_tuner(void); // См. файл "http-tuner.c"
void stop_http_tuner(void);  // См. файл "http-tuner.c"

// См. файл "driver-nt60.c"
void nt60_servo_setup(const char *value, char *answer_buf_ptr, int *answer_len_ptr);

// См. файл "system-info.c"
void get_system_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
void get_temperature_info_string(const char *value, char *answer_buf_ptr, int *answer_len_ptr);
#endif // MAIN_HEADER_GUARD_H
