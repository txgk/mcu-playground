#ifndef NOSEDIVE_H
#define NOSEDIVE_H
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
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

#define WIFI_AP_SSID       "demoproshivka"
#define WIFI_AP_PASS       "elbereth"

#define HTTP_STREAMER_PORT 222
#define HTTP_TUNER_PORT    333
#define HTTP_STREAMER_CTRL 2222
#define HTTP_TUNER_CTRL    3333

#define HEARTBEAT_PERIOD_MS          1000
#define ESP_TEMP_PERIOD_MS           5000
#define WIFI_RECONNECTION_PERIOD_MS  2000
#define BMX280_POLLING_PERIOD_MS     1000
#define TPA626_POLLING_PERIOD_MS     500
#define LIS3DH_POLLING_PERIOD_MS     1000
#define MAX6675_POLLING_PERIOD_MS    1000
#define NTC_POLLING_PERIOD_MS        1000
#define SPEED_POLLING_PERIOD_MS      1000
#define PWM_READER_POLLING_PERIOD_MS 1000
#define POWER_POLLING_PERIOD_MS      2000

#define BME280_I2C_ADDRESS  118
#define TPA626_I2C_ADDRESS  65
#define LIS3DH_I2C_ADDRESS  25
#define PCA9685_I2C_ADDRESS 112
#define BME280_I2C_PORT     I2C_NUM_0
#define TPA626_I2C_PORT     I2C_NUM_0
#define LIS3DH_I2C_PORT     I2C_NUM_0
#define PCA9685_I2C_PORT    I2C_NUM_0

#define LENGTH(A) (sizeof((A))/sizeof(*(A)))
#define MS_TO_TICKS(A) ((A) / portTICK_PERIOD_MS)
#define CYCLES_TO_US(A) ((A) / cpu_frequency_mhz)
#define US_TO_CYCLES(A) ((A) * cpu_frequency_mhz)
#define TASK_DELAY_MS(A) vTaskDelay((A) / portTICK_PERIOD_MS)
#define INIT_IP4_LOL(a, b, c, d) { .type = ESP_IPADDR_TYPE_V4, .u_addr = { .ip4 = { .addr = ESP_IP4TOADDR(a, b, c, d) }}}

enum {
	MUX_I2C_DRIVER = 0,
	MUX_HTTP_STREAMER,
	MUX_HTTP_STREAMER_ACTIVITY_INDICATOR,
	NUMBER_OF_MUTEXES,
};

struct data_piece {
	char *ptr;
	int len;
};

const struct data_piece *tell_esp_to_restart(const char *dummy); // См. файл "main.c"

bool start_http_streamer(void);                                // См. файл "http-streamer.c"
void send_data(const char *new_data_buf, size_t new_data_len); // См. файл "http-streamer.c"
void stop_http_streamer(void);                                 // См. файл "http-streamer.c"
bool start_http_tuner(void);                                   // См. файл "http-tuner.c"
void stop_http_tuner(void);                                    // См. файл "http-tuner.c"

void esp_temp_task(void *dummy);   // См. файл "sensor-esp-temp.c"
void bmx280_task(void *dummy);     // См. файл "sensor-bmx280.c"
void tpa626_task(void *dummy);     // См. файл "sensor-tpa626.c"
void lis3dh_task(void *dummy);     // См. файл "sensor-lis3dh.c"
void max6675_task(void *dummy);    // См. файл "sensor-max6675.c"
void winbond_task(void *dummy);    // См. файл "sensor-w25q128jv.c"
void ntc_task(void *dummy);        // См. файл "sensor-ntc.c"
void speed_task(void *dummy);      // См. файл "sensor-speed.c"
void hall_task(void *dummy);       // См. файл "sensor-hall.c"
void pwm_reader_task(void *dummy); // См. файл "sensor-pwm-reader.c"
void power_task(void *dummy);      // См. файл "sensor-power.c"

// См. файл "sensor-pca9685.c"
const struct data_piece *pca9685_http_handler_pcaset(const char *value);
const struct data_piece *pca9685_http_handler_pcamax(const char *value);
const struct data_piece *pca9685_http_handler_pcaoff(const char *value);
const struct data_piece *pca9685_http_handler_pcafreq(const char *value);

// См. файл "common-i2c.c"
uint8_t i2c_read_one_byte_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);
uint16_t i2c_read_two_bytes_from_register(i2c_port_t port, uint8_t addr, uint8_t reg, long timeout_ms);

// См. файл "driver-pwm-reader.c"
void calculate_pwm(void *gpio_num);
double get_pwm1_frequency(void);
double get_pwm2_frequency(void);
double get_pwm1_duty_cycle(void);
double get_pwm2_duty_cycle(void);

// См. файл "system-info.c"
void create_system_info_string(void);
const struct data_piece *get_system_info_string(const char *dummy);

extern SemaphoreHandle_t system_mutexes[NUMBER_OF_MUTEXES];
extern adc_oneshot_unit_handle_t adc1_handle;
extern adc_oneshot_unit_handle_t adc2_handle;
#endif // NOSEDIVE_H
