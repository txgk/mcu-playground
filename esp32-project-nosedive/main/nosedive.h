#ifndef NOSEDIVE_H
#define NOSEDIVE_H
#include <stdbool.h>
#include "driver/uart.h"
#include "../../wifi-credentials.h"

#define UART0_TX_PIN       1
#define UART0_RX_PIN       3
#define UART0_SPEED        115200
#define UART1_TX_PIN       17
#define UART1_RX_PIN       16
#define UART1_SPEED        115200

#define HTTP_STREAMER_PORT 222
#define HTTP_TUNER_PORT    333
#define HTTP_STREAMER_CTRL 2222
#define HTTP_TUNER_CTRL    3333

#define WIFI_RECONNECTION_PERIOD_MS 2000

#define TASK_DELAY_MS(A) vTaskDelay(A / portTICK_PERIOD_MS)
#define INIT_IP4_LOL(a, b, c, d) { .type = ESP_IPADDR_TYPE_V4, .u_addr = { .ip4 = { .addr = ESP_IP4TOADDR(a, b, c, d) }}}

// Реализация находится в файле "http-streamer.c"
bool start_http_streamer(void);
void stop_http_streamer(void);

// Реализация находится в файле "http-tuner.c"
bool start_http_tuner(void);
void stop_http_tuner(void);

#endif // NOSEDIVE_H
