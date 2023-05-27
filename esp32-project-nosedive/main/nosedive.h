#ifndef NOSEDIVE_H
#define NOSEDIVE_H
#include <stdbool.h>
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "esp_log.h"
// #include "../../wifi-credentials.h"

#define UART0_TX_PIN       1
#define UART0_RX_PIN       3
#define UART0_SPEED        115200
#define UART1_TX_PIN       17
#define UART1_RX_PIN       16
#define UART1_SPEED        115200
#define I2C1_SDA_PIN       27
#define I2C1_SCL_PIN       26
#define I2C1_SPEED         1000000
#define NTC_PIN            33

#define HTTP_STREAMER_PORT 222
#define HTTP_TUNER_PORT    333
#define HTTP_STREAMER_CTRL 2222
#define HTTP_TUNER_CTRL    3333

#define HEARTBEAT_PERIOD_MS         1000
#define WIFI_RECONNECTION_PERIOD_MS 2000
#define BMX280_POLLING_PERIOD_MS    1000
#define TPA626_POLLING_PERIOD_MS    1000
#define LIS3DH_POLLING_PERIOD_MS    1000
#define NTC_POLLING_PERIOD_MS       1000

#define BME280_I2C_ADDRESS 118
#define TPA626_I2C_ADDRESS 65
#define LIS3DH_I2C_ADDRESS 25

#define TASK_DELAY_MS(A) vTaskDelay((A) / portTICK_PERIOD_MS)
#define INIT_IP4_LOL(a, b, c, d) { .type = ESP_IPADDR_TYPE_V4, .u_addr = { .ip4 = { .addr = ESP_IP4TOADDR(a, b, c, d) }}}

enum {
	MUX_I2C_DRIVER = 0,
	MUX_HTTP_STREAMER,
	MUX_HTTP_STREAMER_ACTIVITY_INDICATOR,
	NUMBER_OF_MUTEXES,
};

bool start_http_streamer(void);                                // См. файл "http-streamer.c"
void send_data(const char *new_data_buf, size_t new_data_len); // См. файл "http-streamer.c"
void stop_http_streamer(void);                                 // См. файл "http-streamer.c"
bool start_http_tuner(void);                                   // См. файл "http-tuner.c"
void stop_http_tuner(void);                                    // См. файл "http-tuner.c"

void heartbeat_task(void *dummy); // См. файл "heartbeat.c"
void bmx280_task(void *dummy);    // См. файл "sensor-bmx280.c"
void tpa626_task(void *dummy);    // См. файл "sensor-tpa626.c"
void lis3dh_task(void *dummy);    // См. файл "sensor-lis3dh.c"
void init_adc_for_ntc_task(void); // См. файл "sensor-ntc.c"
void ntc_task(void *dummy);       // См. файл "sensor-ntc.c"

// См. файл "adc-common.c"
bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

extern SemaphoreHandle_t system_mutexes[NUMBER_OF_MUTEXES];
extern adc_oneshot_unit_handle_t adc1_handle;
#endif // NOSEDIVE_H
