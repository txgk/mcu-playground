#ifndef NOSEDIVE_H
#define NOSEDIVE_H
#include <stdatomic.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
// #define NOSEDIVE_USE_WIFI_STATION
// #define READ_HALL_AS_ANALOG_PIN
#ifdef NOSEDIVE_USE_WIFI_STATION
#include "../../wifi-credentials.h"
#endif

#define FIRMWARE_CODEWORD  "Пономарь"

#define UART0_TX_PIN       1
#define UART0_RX_PIN       3
#define UART0_SPEED        115200
#define UART1_TX_PIN       17
#define UART1_RX_PIN       16
#define UART1_SPEED        115200
#define I2C1_SDA_PIN       27
#define I2C1_SCL_PIN       26
#define I2C1_SPEED         1000000
#define SPI1_MOSI_PIN      23
#define SPI1_MISO_PIN      19
#define SPI1_SCLK_PIN      18
#define MAX6675_CS_PIN     0
#define MAX6675_SPI_HOST   SPI2_HOST
#define WINBOND_CS_PIN     2
#define WINBOND_SPI_HOST   SPI2_HOST
#define PWM1_INPUT_PIN     34
#define PWM1_INPUT_PIN_REG GPIO_PIN_REG_34
#define PWM2_INPUT_PIN     35
#define PWM2_INPUT_PIN_REG GPIO_PIN_REG_35
#define KILL_SWITCH_PIN    36
#define POWER_ON_PIN       39
#define CURRENT_ALERT_PIN  5
#define HALL_PIN           25
#define HALL_ADC_UNIT      ADC_UNIT_2
#define HALL_ADC_CHANNEL   8
#define NTC_PIN            33
#define NTC_ADC_UNIT       ADC_UNIT_1
#define NTC_ADC_CHANNEL    ADC_CHANNEL_5 // Потому что это GPIO 33
#define SPEEP_PIN          32
#define SPEED_ADC_UNIT     ADC_UNIT_1
#define SPEED_ADC_CHANNEL  ADC_CHANNEL_4 // Потому что это GPIO 32
// Связь ADC каналов с GPIO упоминается в soc/esp32/include/soc/adc_channel.h

#define BME280_I2C_ADDRESS  118
#define TPA626_I2C_ADDRESS  65
#define LIS3DH_I2C_ADDRESS  25
#define PCA9685_I2C_ADDRESS 112
#define BME280_I2C_PORT     I2C_NUM_0
#define TPA626_I2C_PORT     I2C_NUM_0
#define LIS3DH_I2C_PORT     I2C_NUM_0
#define PCA9685_I2C_PORT    I2C_NUM_0

#define WIFI_AP_SSID               "demoproshivka"
#define WIFI_AP_PASS               "elbereth"
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

#define MESSAGE_SIZE_LIMIT 500

struct data_piece {
	char *ptr;
	int len;
};

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

const struct data_piece *tell_esp_to_restart(const char *dummy); // См. файл "main.c"

void start_tasks(void); // См. файл "tasks.c"

bool start_websocket_streamer(void); // См. файл "streamer-websocket.c"
bool start_serial_streamer(void);    // См. файл "streamer-serial.c"

bool start_http_tuner(void); // См. файл "http-tuner.c"
void stop_http_tuner(void);  // См. файл "http-tuner.c"

// См. файл "common-i2c.c"
uint8_t i2c_read_one_byte_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);
uint16_t i2c_read_two_bytes_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);

// См. файл "system-info.c"
void create_system_info_string(void);
const struct data_piece *get_system_info_string(const char *dummy);
const struct data_piece *get_temperature_info_string(const char *dummy);

extern struct task_descriptor tasks[];
extern adc_oneshot_unit_handle_t adc1_handle;
extern adc_oneshot_unit_handle_t adc2_handle;
#endif // NOSEDIVE_H
