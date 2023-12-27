#include <string.h>
// #include <esp_types.h>
// #include <stdatomic.h>
// #include <util/atomic.h>
#include <Arduino.h>
#include <WiFi.h>
// #include <ArduinoOTA.h>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "HX711.h"
#include "WebSocketsServer.h"
#include "main.h"
#include "../../wifi-credentials.h"

IPAddress            ip(192, 168, 102, 41);
IPAddress       gateway(192, 168, 102, 99);
IPAddress        subnet(255, 255, 255,  0);
IPAddress   primary_dns( 77,  88,   8,  8);
IPAddress secondary_dns( 77,  88,   8,  1);

WebSocketsServer ws = WebSocketsServer(WS_STREAMER_PORT);

#define JETCAT_CAN_ID_BASE               256

#define LOG_SERIAL_SPEED                 9600
#define SERIAL_SPEED                     9600

#define SDA                              ((REG_READ(GPIO_IN1_REG)) & 0b0001)
#define SCL                              ((REG_READ(GPIO_IN1_REG)) & 0b0010)
#define SDA2                             ((REG_READ(GPIO_IN1_REG)) & 0b0100)
#define SCL2                             ((REG_READ(GPIO_IN1_REG)) & 0b1000)
#define SCL_OR_SDA                       ((REG_READ(GPIO_IN1_REG)) & 0b0011)
#define SCL2_OR_SDA2                     ((REG_READ(GPIO_IN1_REG)) & 0b1100)
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA   == 0b0010)
#define SDA2_IS_LOW_AND_SCL2_IS_HIGH     (SCL2_OR_SDA2 == 0b1000)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA   == 0b0011)
#define SDA2_AND_SCL2_ARE_HIGH           (SCL2_OR_SDA2 == 0b1100)
#define IS_ACKNOWLEDGED                  '>'
#define NOT_ACKNOWLEDGED                 'x'

#define WAIT_SDA_RISE                    while (SDA  == 0)
#define WAIT_SDA2_RISE                   while (SDA2 == 0)
#define WAIT_SDA_DROP                    while (SDA  != 0)
#define WAIT_SDA2_DROP                   while (SDA2 != 0)
#define WAIT_SCL_RISE                    while (SCL  == 0)
#define WAIT_SCL2_RISE                   while (SCL2 == 0)
#define WAIT_SCL_DROP                    while (SCL  != 0)
#define WAIT_SCL2_DROP                   while (SCL2 != 0)
#define WAIT_FOR_SDA_AND_SCL_RISE        while (SCL_OR_SDA   != 0b0011)
#define WAIT_FOR_SDA2_AND_SCL2_RISE      while (SCL2_OR_SDA2 != 0b1100)
#define WAIT_FOR_SDA_OR_SCL_DROP         while (SCL_OR_SDA   == 0b0011)
#define WAIT_FOR_SDA2_OR_SCL2_DROP       while (SCL2_OR_SDA2 == 0b1100)
#define WAIT_FOR_SDA_RISE_OR_SCL_DROP    while (SCL_OR_SDA   == 0b0010)
#define WAIT_FOR_SDA2_RISE_OR_SCL2_DROP  while (SCL2_OR_SDA2 == 0b1000)

#define I2C1_BUFFER_SIZE                 4000
#define I2C2_BUFFER_SIZE                 4000
#define UART_BUFFER_SIZE                 1000

#define HEART_BEAT_PERIODICITY           2000 // milliseconds

struct instruction_entry {
	char *data;
	size_t data_len;
	bool enabled;
};

void start_i2c1_loop(void);
void start_i2c2_loop(void);
void start_uart_loop(void);
void start_beat_loop(void);
void start_can_loop(void);

SemaphoreHandle_t wifi_lock = NULL;

volatile uint8_t b, c;
char i2c1[I2C1_BUFFER_SIZE], i2c2[I2C2_BUFFER_SIZE];
volatile size_t i2c1_len = 0, i2c2_len = 0;

char engine_id = '1';

int esc_mode = 0, new_esc_filling = 0;

char **fast_queue = NULL;
volatile size_t fast_queue_pos = 0;
volatile size_t fast_queue_len = 0;
SemaphoreHandle_t fast_queue_lock = NULL;

volatile struct instruction_entry *uart_circle = NULL;
volatile size_t uart_circle_pos = 0;
volatile size_t uart_circle_len = 0;
SemaphoreHandle_t uart_circle_lock = NULL;

char uart[UART_BUFFER_SIZE];
volatile size_t uart_len = 0;

volatile bool i2c1_allowed_to_run = true, i2c1_has_stopped = false;
volatile bool i2c2_allowed_to_run = true, i2c2_has_stopped = false;
volatile bool uart_allowed_to_run = true, uart_has_stopped = false;
volatile bool beat_allowed_to_run = true, beat_has_stopped = false;

volatile long faked_rpm = 0;
SemaphoreHandle_t rpm_lock = NULL;

ledc_timer_config_t timer_config = {
	.speed_mode = LEDC_HIGH_SPEED_MODE,
	.duty_resolution = LEDC_TIMER_13_BIT,
	.timer_num = LEDC_TIMER_0,
	.freq_hz = 50,
	.clk_cfg = LEDC_USE_REF_TICK, // 1 MHz, high speed, frequency scaling
	// .deconfigure = false,
};
ledc_channel_config_t channel_config = {
	.gpio_num = PWM_OUT_1_PIN,
	.speed_mode = LEDC_HIGH_SPEED_MODE,
	.channel = LEDC_CHANNEL_0,
	.intr_type = LEDC_INTR_DISABLE,
	.timer_sel = LEDC_TIMER_0,
	// .duty = 820, // Заполнение ~10% для имитации датчика Холла.
	.duty = 622, // Заполнение ~7.5% для имитации датчика Холла.
	.hpoint = 0,
};

HX711 loadcell;

#define RSHD_SAMPLES_COUNT         11

// static volatile _Atomic int64_t rshd_samples[RSHD_SAMPLES_COUNT];
// static volatile _Atomic int64_t rshd_period_1;
// static volatile _Atomic int64_t rshd_period_3;
// static volatile _Atomic int64_t rshd_period_10;
// static volatile _Atomic size_t rshd_iter = 0;
static volatile int64_t rshd_samples[RSHD_SAMPLES_COUNT];
static volatile int64_t rshd_period_1;
static volatile int64_t rshd_period_3;
static volatile int64_t rshd_period_10;
static volatile size_t rshd_iter = 0;

void IRAM_ATTR
rshd_take_sample(void *dummy)
{
	rshd_samples[rshd_iter] = esp_timer_get_time();
	if (rshd_iter == 0) {
		// _0 1 2 3 4 5 6 7 8 9 10_
		rshd_period_1 = rshd_samples[rshd_iter] - rshd_samples[RSHD_SAMPLES_COUNT - 1];
	} else {
		// 0 1 2 3 4 5_6 7 8 9 10
		rshd_period_1 = rshd_samples[rshd_iter] - rshd_samples[rshd_iter - 1];
	}
	if (rshd_iter < 3) {
		// _0_1 2 3 4 5 6 7 8 9_10_
		rshd_period_3 = (rshd_samples[rshd_iter] - rshd_samples[RSHD_SAMPLES_COUNT - (3 - rshd_iter)]) / 3;
	} else {
		// 0 1 2 3_4_5_6 7 8 9 10
		rshd_period_3 = (rshd_samples[rshd_iter] - rshd_samples[rshd_iter - 3]) / 3;
	}
	if (rshd_iter < 10) {
		// _0_1_2 3_4_5_6_7_8_9_10_
		rshd_period_10 = (rshd_samples[rshd_iter] - rshd_samples[RSHD_SAMPLES_COUNT - (10 - rshd_iter)]) / 10;
	} else {
		// 0_1_2_3_4_5_6_7_8_9_10
		rshd_period_10 = (rshd_samples[rshd_iter] - rshd_samples[rshd_iter - 10]) / 10;
	}
	rshd_iter = (rshd_iter + 1) % RSHD_SAMPLES_COUNT;
}

void
add_command_to_fast_queue(const char *cmd, size_t cmd_len)
{
	if (xSemaphoreTake(fast_queue_lock, portMAX_DELAY) == pdTRUE) {
		char **tmp = (char **)realloc(fast_queue, sizeof(char *) * (fast_queue_len + 1));
		if (tmp != NULL) {
			fast_queue = tmp;
			fast_queue[fast_queue_len] = (char *)malloc(sizeof(char) * (cmd_len + 1));
			memcpy(fast_queue[fast_queue_len], cmd, cmd_len);
			fast_queue[fast_queue_len][cmd_len] = '\0';
			fast_queue_len += 1;
		}
		xSemaphoreGive(fast_queue_lock);
	}
}

bool
try_to_write_next_command_from_fast_queue_to_serial(void)
{
	bool wrote = false;
	if (xSemaphoreTake(fast_queue_lock, portMAX_DELAY) == pdTRUE) {
		if ((fast_queue != NULL) && (fast_queue_len != 0)) {
			if (fast_queue_pos == fast_queue_len) {
				free(fast_queue);
				fast_queue = NULL;
				fast_queue_pos = 0;
				fast_queue_len = 0;
			} else {
				wrote = true;
				Serial1.write(fast_queue[fast_queue_pos]);
				Serial1.write('\r');
				fast_queue_pos += 1;
			}
		}
		xSemaphoreGive(fast_queue_lock);
	}
	return wrote;
}

void
add_command_to_uart_circle(const char *cmd, size_t cmd_len, bool enable)
{
	if (xSemaphoreTake(uart_circle_lock, portMAX_DELAY) == pdTRUE) {
		for (size_t i = 0; i < uart_circle_len; ++i) {
			if ((cmd_len == uart_circle[i].data_len) && (strncmp(cmd, uart_circle[i].data, cmd_len) == 0)) {
				uart_circle[i].enabled = enable;
				xSemaphoreGive(uart_circle_lock);
				return;
			}
		}
		struct instruction_entry *tmp = (struct instruction_entry *)realloc((void *)uart_circle, sizeof(struct instruction_entry) * (uart_circle_len + 1));
		if (tmp != NULL) {
			tmp[uart_circle_len].data = (char *)malloc(sizeof(char) * (cmd_len + 1));
			memcpy(tmp[uart_circle_len].data, cmd, cmd_len);
			tmp[uart_circle_len].data[cmd_len] = '\0';
			tmp[uart_circle_len].data_len = cmd_len;
			tmp[uart_circle_len].enabled = enable;
			uart_circle = tmp;
			uart_circle_len += 1;
		}
		xSemaphoreGive(uart_circle_lock);
	}
}

bool
try_to_write_next_command_from_uart_circle_to_serial(void)
{
	bool status = false;
	if (xSemaphoreTake(uart_circle_lock, portMAX_DELAY) == pdTRUE) {
		for (size_t i = 0; i < uart_circle_len; ++i) {
			uart_circle_pos = (uart_circle_pos + 1) % uart_circle_len;
			if (uart_circle[uart_circle_pos].enabled) {
				char buf[50];
				snprintf(buf, 50, "1,%s,%c\r", uart_circle[uart_circle_pos].data, engine_id);
				Serial1.write(buf);
				status = true;
				break;
			}
		}
		xSemaphoreGive(uart_circle_lock);
	}
	return status;
}

void
send_data(const char *data, size_t data_len)
{
	if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
		if (ws.connectedClients() > 0) ws.broadcastTXT(data, data_len);
		xSemaphoreGive(wifi_lock);
	}
}

void IRAM_ATTR
i2c1_loop(void *dummy)
{
i2c1_sync:
	WAIT_FOR_SDA_AND_SCL_RISE;
i2c1_idle:
	WAIT_FOR_SDA_OR_SCL_DROP;
	if (!SDA_IS_LOW_AND_SCL_IS_HIGH) goto i2c1_sync;
i2c1_start:
	if (i2c1_allowed_to_run == false) {
		i2c1_has_stopped = true;
		vTaskDelete(NULL);
	}
	i2c1_len = sprintf(i2c1, "\nI2C1@%lu=", millis());
	WAIT_SCL_DROP;
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	i2c1_len += sprintf(i2c1 + i2c1_len, "%" PRIu8, b);
	if (SDA) {
		WAIT_SCL_DROP;
		i2c1[i2c1_len++] = NOT_ACKNOWLEDGED;
		send_data(i2c1, i2c1_len);
		goto i2c1_sync;
	} else {
		WAIT_SCL_DROP;
		i2c1[i2c1_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
i2c1_next:
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	i2c1_len += sprintf(i2c1 + i2c1_len, "%" PRIu8, b);
	if (SDA) {
		i2c1[i2c1_len++] = NOT_ACKNOWLEDGED;
	} else {
		i2c1[i2c1_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_DROP;

	while (SCL == 0) b = SDA;
	if (b) {
		WAIT_FOR_SDA_OR_SCL_DROP;
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) { // Repeated start signal.
			goto i2c1_start;
		}
	} else {
		WAIT_FOR_SDA_RISE_OR_SCL_DROP;
		if (SDA_AND_SCL_ARE_HIGH) { // Stop signal.
			send_data(i2c1, i2c1_len);
			goto i2c1_sync;
		}
	}

	b <<= 1;
	WAIT_SCL_DROP;
	goto i2c1_next;
}

void IRAM_ATTR
i2c2_loop(void *dummy)
{
i2c2_sync:
	WAIT_FOR_SDA2_AND_SCL2_RISE;
i2c2_idle:
	WAIT_FOR_SDA2_OR_SCL2_DROP;
	if (!SDA2_IS_LOW_AND_SCL2_IS_HIGH) goto i2c2_sync;
i2c2_start:
	if (i2c2_allowed_to_run == false) {
		i2c2_has_stopped = true;
		vTaskDelete(NULL);
	}
	i2c2_len = sprintf(i2c2, "\nI2C2@%lu=", millis());
	WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c  = SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2;          WAIT_SCL2_DROP;
	WAIT_SCL2_RISE;
	i2c2_len += sprintf(i2c2 + i2c2_len, "%" PRIu8, c);
	if (SDA2) {
		WAIT_SCL2_DROP;
		i2c2[i2c2_len++] = NOT_ACKNOWLEDGED;
		send_data(i2c2, i2c2_len);
		goto i2c2_sync;
	} else {
		WAIT_SCL2_DROP;
		i2c2[i2c2_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL2_RISE; c  = SDA2; c <<= 1; WAIT_SCL2_DROP;
i2c2_next:
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2;          WAIT_SCL2_DROP;
	WAIT_SCL2_RISE;
	i2c2_len += sprintf(i2c2 + i2c2_len, "%" PRIu8, c);
	if (SDA2) {
		i2c2[i2c2_len++] = NOT_ACKNOWLEDGED;
	} else {
		i2c2[i2c2_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL2_DROP;

	while (SCL2 == 0) c = SDA2;
	if (c) {
		WAIT_FOR_SDA2_OR_SCL2_DROP;
		if (SDA2_IS_LOW_AND_SCL2_IS_HIGH) { // Repeated start signal.
			goto i2c2_start;
		}
	} else {
		WAIT_FOR_SDA2_RISE_OR_SCL2_DROP;
		if (SDA2_AND_SCL2_ARE_HIGH) { // Stop signal.
			send_data(i2c2, i2c2_len);
			goto i2c2_sync;
		}
	}

	c <<= 1;
	WAIT_SCL2_DROP;
	goto i2c2_next;
}

void IRAM_ATTR
uart_loop(void *dummy)
{
	while (uart_allowed_to_run == true) {
		if (try_to_write_next_command_from_fast_queue_to_serial() == false) {
			if (try_to_write_next_command_from_uart_circle_to_serial() == false) {
				TASK_DELAY_MS(50);
				continue;
			}
		}
		size_t packet_ends = 0;
		unsigned long packet_birth = millis();
		size_t uart_prefix_len = snprintf(uart, UART_BUFFER_SIZE, "UART@%lu=", packet_birth);
		uart_len = uart_prefix_len;
		while (true) {
			if (Serial1.available() > 0) {
				if (uart_len >= UART_BUFFER_SIZE) {
					Serial1.read(); // Ignore bytes for which there is no room.
					continue;
				}
				uart[uart_len++] = Serial1.read();
				if (uart[uart_len - 1] == '\r') {
					uart[uart_len - 1] = '~';
					packet_ends += 1;
					if (packet_ends > 1) {
						uart[uart_len++] = '\n';
						send_data(uart, uart_len);
						if (strstr(uart + uart_prefix_len, "1,RAC") == uart + uart_prefix_len) {
							// parse RAC
						} else if (strstr(uart + uart_prefix_len, "1,RFI") == uart + uart_prefix_len) {
							// parse RFI
						}
						if ((uart_len - uart_prefix_len > 25)
							&& (strncmp(uart + uart_prefix_len, "1,RAC,1~1,HS,OK,", 16) == 0))
						{
							if (xSemaphoreTake(rpm_lock, portMAX_DELAY) == pdTRUE) {
								faked_rpm = 0;
								for (const char *i = uart + uart_prefix_len + 16; ISDIGIT(*i); ++i) {
									faked_rpm = faked_rpm * 10 + (*i - '0');
								}
								// char shft[100];
								// size_t shft_len;
								// shft_len = snprintf(shft, 100, "SHFT=%lu\n", faked_rpm);
								// if (shft_len < 100) {
								// 	send_data(shft, shft_len);
								// }
								xSemaphoreGive(rpm_lock);
							}
						}
						break;
					}
				}
			} else if (millis() > packet_birth + 2000) {
				break;
			} else {
				TASK_DELAY_MS(50);
			}
		}
		// Discard leftovers.
		while (Serial1.available() > 0) {
			Serial1.read();
		}
	}
	uart_has_stopped = true;
	vTaskDelete(NULL);
}

void IRAM_ATTR
beat_loop(void *dummy)
{
	char beat_buf[100];
	unsigned i = 1;
	while (beat_allowed_to_run == true) {
		int beat_len = sprintf(beat_buf, "BEAT@%lu=%u\n", millis(), i);
		if (beat_len > 0 && beat_len < 100) send_data(beat_buf, beat_len);
		i = (i * 10) % 9999;
		TASK_DELAY_MS(HEART_BEAT_PERIODICITY);
	}
	beat_has_stopped = true;
	vTaskDelete(NULL);
}

void
uart_send_command(const char *value)
{
	add_command_to_fast_queue(value, strlen(value));
	http_tuner_response("Queued command: %s\n", value);
}

void
set_engine_id_command(const char *value)
{
	if (strlen(value) > 0) {
		engine_id = value[0];
		http_tuner_response("Engine ID set: %c\n", value[0]);
	}
}

void
set_esc_pwm_command(const char *value)
{
	int pwm = 622;
	if (sscanf(value, "%d", &pwm) == 1) {
		new_esc_filling = pwm;
		http_tuner_response("ESC PWM set: %d\n", pwm);
	}
}

void
enable_ecu_telemetry_command(const char *value)
{
	add_command_to_uart_circle(value, strlen(value), true);
	http_tuner_response("Added ECU command to UART circle: %s\n", value);
}

void
disable_ecu_telemetry_command(const char *value)
{
	add_command_to_uart_circle(value, strlen(value), false);
	http_tuner_response("Removed ECU command from UART circle: %s\n", value);
}

void IRAM_ATTR
tachometer_faker_loop(void *dummy)
{
	long current_rpm = 0;
	while (true) {
		if (current_rpm != faked_rpm) {
			if (xSemaphoreTake(rpm_lock, portMAX_DELAY) == pdTRUE) {
				current_rpm = faked_rpm;
				xSemaphoreGive(rpm_lock);
			}
			// Делим на 60, т. к. 10 Гц = 600 об/м
			// Делим на 20, т. к. max(rpm1) = 160, max(rpm2) = 8
			// ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, current_rpm / 60 / 20);
			timer_config.freq_hz = current_rpm / 60 / 20;
			ledc_timer_config(&timer_config);
			// char shft[100];
			// size_t shft_len = snprintf(shft, 100, "SHFT=%lu\n", current_rpm / 60 / 20);
			// if (shft_len < 100) send_data(shft, shft_len);
		}
		TASK_DELAY_MS(50);
	}
	// timer_config.deconfigure = true;
	// ledc_timer_config(&timer_config);
	// ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
	vTaskDelete(NULL);
}

void IRAM_ATTR
loadcell_and_rshd_loop(void *dummy)
{
	char loadcell_and_rshd_buf[400];
	while (true) {
		unsigned long packet_birth = millis();
		if (loadcell.is_ready()) {
			long value = loadcell.read();
			int loadcell_and_rshd_buf_len = snprintf(
				loadcell_and_rshd_buf,
				400,
				"\nTHR@%lu=%ld\nRSHD@%lu=%lld,%lld,%lld",
				packet_birth,
				value,
				packet_birth,
				rshd_period_1,
				rshd_period_3,
				rshd_period_10
			);
			if (loadcell_and_rshd_buf_len > 0 && loadcell_and_rshd_buf_len < 400) {
				send_data(loadcell_and_rshd_buf, loadcell_and_rshd_buf_len);
			}
		} else {
			int loadcell_and_rshd_buf_len = snprintf(
				loadcell_and_rshd_buf,
				400,
				"RSHD@%lu=%lld,%lld,%lld\n",
				packet_birth,
				rshd_period_1,
				rshd_period_3,
				rshd_period_10
			);
			if (loadcell_and_rshd_buf_len > 0 && loadcell_and_rshd_buf_len < 400) {
				send_data(loadcell_and_rshd_buf, loadcell_and_rshd_buf_len);
			}
		}
		TASK_DELAY_MS(100);
	}
	vTaskDelete(NULL);
}

void IRAM_ATTR
can_loop(void *dummy)
{
	char out[1000];
	twai_message_t message;
	while (true) {
		if (twai_receive(&message, pdMS_TO_TICKS(10000)) == ESP_OK) {
			// Serial.printf("Received message %d in %s format\n", message.identifier, message.extd ? "extended" : "standard");
			if (!(message.rtr)) {
				unsigned long packet_birth = millis();
				if ((message.identifier == JETCAT_CAN_ID_BASE + 0) && (message.data_length_code == 8)) {
					unsigned long set_rpm = (unsigned long)(((unsigned int)message.data[0] << 8) + message.data[1]) * 10; // rpm
					unsigned long real_rpm = (unsigned long)(((unsigned int)message.data[2] << 8) + message.data[3]) * 10; // rpm
					float exhaust_gas_temp = (float)(((unsigned int)message.data[4] << 8) + message.data[5]) / 10.0; // celsius degree
					int engine_state = message.data[6]; // flags
					float pump_volts = (float)message.data[7] / 10.0; // volts
					int len = snprintf(out, 1000,
						"CAN_VALUES@%lu=%lu,%lu,%.1f,%d,%.1f\n",
						packet_birth,
						set_rpm,
						real_rpm,
						exhaust_gas_temp,
						engine_state,
						pump_volts
					);
					if (len > 0 && len < 1000) send_data(out, len);
				} else if ((message.identifier == JETCAT_CAN_ID_BASE + 1) && (message.data_length_code == 6)) {
					float bat_volts = (float)(message.data[0]) / 10.0; // volts
					float engine_amps = (float)(message.data[1]) / 10.0; // amps
					float generator_volts = (float)(message.data[2]) / 5.0; // volts
					float generator_amps = (float)(message.data[3]) / 10.0; // amps
					int len = snprintf(out, 1000,
						"CAN_ELECTRO@%lu=%.1f,%.1f,%.1f,%.1f,%d\n",
						packet_birth,
						bat_volts,
						engine_amps,
						generator_volts,
						generator_amps,
						message.data[4]
						// message.data[4] & 1  ? 1 : 0, // engine_connected
						// message.data[4] & 2  ? 1 : 0, // fuel_pump_connected
						// message.data[4] & 4  ? 1 : 0, // smoke_pump_connected
						// message.data[4] & 8  ? 1 : 0, // gsu_connected
						// message.data[4] & 16 ? 1 : 0, // egt_ok_flag
						// message.data[4] & 32 ? 1 : 0, // glow_plug_ok_flag
						// message.data[4] & 64 ? 1 : 0  // flow_sensor_ok_flag
					);
					if (len > 0 && len < 1000) send_data(out, len);
				} else if ((message.identifier == JETCAT_CAN_ID_BASE + 2) && (message.data_length_code == 8)) {
					unsigned long fuel_flow = (unsigned long)(((int)message.data[0] << 8) + message.data[1]); // ml/min
					unsigned long fuel_consum = (unsigned long)(((int)message.data[2] << 8) + message.data[3]) * 10; // ml
					float air_pressure = (float)(((unsigned int)message.data[4] << 8) + message.data[5]) / 50.0; // mbar
					float ecu_temp = (float)message.data[6] / 2.0 + 30; // celsius degree
					int len = snprintf(out, 1000,
						"CAN_FUEL@%lu=%lu,%lu,%.1f,%.1f\n",
						packet_birth,
						fuel_flow,
						fuel_consum,
						air_pressure,
						ecu_temp
					);
					if (len > 0 && len < 1000) send_data(out, len);
				} else {
					// for (int i = 0; i < message.data_length_code; i++) {
						// Serial.printf("Data byte %d = %d\n", i, message.data[i]);
					// }
				}
			}
		} else {
			// Serial.println("Message reception timed out!");
		}
	}
	vTaskDelete(NULL);
}

void IRAM_ATTR
adc_to_pwm_faker_loop(void *dummy)
{
	char out[200];
	size_t phase = 0;
	while (true) {
		int voltage = analogRead(BRUSHED_MOTOR_REGULATOR_PIN); // [0; 4095]
		if (esc_mode != new_esc_filling) {
			esc_mode = new_esc_filling;
			channel_config.duty = esc_mode;
			ledc_channel_config(&channel_config);
			// 614 (1.50ms)
			// 618 (1.51ms)
			// 622 (1.52ms)
			// 626 (1.53ms)
			// 630 (1.54ms)
			// 634 (1.55ms)
			// 638 (1.56ms)
			// 642 (1.57ms)
			// 646 (1.58ms)
			// 650 (1.59ms)
		}
		int len = snprintf(out, 200,
			"TEST => VOLTAGE: %5d, DUTY: %5d\n",
			voltage,
			channel_config.duty
		);
		if (len > 0 && len < 200) send_data(out, len);
		TASK_DELAY_MS(100);
		// phase += 1;
		// if ((phase % 100) == 0) {
		// 	phase = 0;
		// 	mode = (mode + 1) % 3;
		// }
	}
	vTaskDelete(NULL);
}

void
start_adc_to_pwm_faker_loop(void)
{
	xTaskCreatePinnedToCore(
		&adc_to_pwm_faker_loop,
		"adc_to_pwm_faker_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_i2c1_loop(void)
{
	xTaskCreatePinnedToCore(
		&i2c1_loop,
		"i2c1_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_i2c2_loop(void)
{
	xTaskCreatePinnedToCore(
		&i2c2_loop,
		"i2c2_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_uart_loop(void)
{
	xTaskCreatePinnedToCore(
		&uart_loop,
		"uart_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_beat_loop(void)
{
	xTaskCreatePinnedToCore(
		&beat_loop,
		"beat_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_tachometer_faker_loop(void)
{
	xTaskCreatePinnedToCore(
		&tachometer_faker_loop,
		"tachometer_faker_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_loadcell_and_rshd_loop(void)
{
	xTaskCreatePinnedToCore(
		&loadcell_and_rshd_loop,
		"loadcell_and_rshd_loop",
		4096, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_can_loop(void)
{
	xTaskCreatePinnedToCore(
		&can_loop,
		"can_loop",
		8192, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
ws_event_handler(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
	if (type == WStype_CONNECTED) {
		ws.broadcastTXT("BEAT@names=HeartBeat(hit)\n", 26);
		ws.broadcastTXT("CAN_VALUES@names=SetRpm(rpm),RealRpm(rpm),EGT(C),EngineState(enum),PumpVoltage(V)\n", 82);
		ws.broadcastTXT("CAN_ELECTRO@names=BatVoltage(V),EngineCurrent(A),GeneratorVoltage(V),GeneratorCurrent(A),ElectroState(bitfield)\n", 112);
		ws.broadcastTXT("CAN_FUEL@names=FuelFlow(ml/min),FuelConsumed(ml),AirPressure(mbar),EcuTemperature(C)\n", 85);
	}
}

void
setup(void)
{
	gpio_install_isr_service(0);
	// gpio_config_t input_cfg = {
	// 	(1ULL << SDA_PIN) | (1ULL << SCL_PIN) | (1ULL << SDA2_PIN) | (1ULL << SCL2_PIN),
	// 	GPIO_MODE_INPUT,
	// 	GPIO_PULLUP_DISABLE,
	// 	GPIO_PULLDOWN_DISABLE,
	// 	GPIO_INTR_DISABLE,
	// };
	// gpio_config(&input_cfg);
	// gpio_config_t rshd_input_cfg = {
	// 	.pin_bit_mask = (1ULL << RASHODOMER_PIN),
	// 	.mode = GPIO_MODE_INPUT,
	// 	.pull_up_en = GPIO_PULLUP_DISABLE,
	// 	.pull_down_en = GPIO_PULLDOWN_DISABLE,
	// 	.intr_type = GPIO_INTR_POSEDGE,
	// };
	// gpio_config(&rshd_input_cfg);
	// gpio_isr_handler_add((gpio_num_t)RASHODOMER_PIN, &rshd_take_sample, NULL);
	ledc_timer_config(&timer_config);
	ledc_channel_config(&channel_config);
	wifi_lock = xSemaphoreCreateMutex();
	fast_queue_lock = xSemaphoreCreateMutex();
	uart_circle_lock = xSemaphoreCreateMutex();
	rpm_lock = xSemaphoreCreateMutex();
	// Serial.begin(LOG_SERIAL_SPEED);
	Serial1.begin(SERIAL_SPEED, SERIAL_8N1, RX_PIN, TX_PIN);
	// loadcell.begin(LOADCELL_SDA_PIN, LOADCELL_SCL_PIN);
	add_command_to_uart_circle("RAC", 3, true);
	add_command_to_uart_circle("RFI", 3, true);
	// WiFi.softAP("HahaHoho", "12345678");
	WiFi.config(ip, gateway, subnet, primary_dns, secondary_dns);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	// while (WiFi.status() != WL_CONNECTED) {
	// 	delay(500);
	// }
	// ArduinoOTA.begin();
	ws.begin();
	ws.onEvent(ws_event_handler);
	// start_i2c1_loop();
	// start_i2c2_loop();
	start_uart_loop();
	start_beat_loop();
	// start_adc_to_pwm_faker_loop();
	start_tachometer_faker_loop();
	// start_loadcell_and_rshd_loop();
	//twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
	//twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
	//twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
	//if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
	//	//Start TWAI driver
	//	if (twai_start() == ESP_OK) {
	//		// Serial.println("Driver started");
	//		start_can_loop();
	//	} else {
	//		// Serial.println("Driver not started");
	//	}
	//} else {
	//	// Serial.println("Driver setup failed");
	//}
	// Serial.println(WiFi.localIP());
	http_tuner_start();
}

void
loop(void)
{
	// if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
	// 	ArduinoOTA.handle();
	// 	xSemaphoreGive(wifi_lock);
	// }
	ws.loop();
}
