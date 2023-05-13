#include <string.h>
#include "driver/ledc.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "../../wifi-credentials.h"

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SDA2_PIN                         34
#define SCL2_PIN                         35
#define RX_PIN                           16
#define TX_PIN                           17
#define PWM_IN_1_PIN                     18
#define PWM_IN_2_PIN                     19
#define PWM_OUT_1_PIN                    4
#define SERIAL_SPEED                     9600
#define WIFI_DATA_PORT                   80
#define WIFI_CTRL_PORT                   81
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

#define TASK_DELAY_MS(A) vTaskDelay(A / portTICK_PERIOD_MS)
#define ISDIGIT(A) (((A)=='0')||((A)=='1')||((A)=='2')||((A)=='3')||((A)=='4')||((A)=='5')||((A)=='6')||((A)=='7')||((A)=='8')||((A)=='9'))

struct instruction_entry {
	char *data;
	size_t data_len;
	bool enabled;
};

void start_i2c1_loop(void);
void start_i2c2_loop(void);
void start_uart_loop(void);
void start_beat_loop(void);

IPAddress            ip(192, 168, 102,  41);
IPAddress       gateway(192, 168, 102,  99);
IPAddress        subnet(255, 255, 255,   0);
IPAddress   primary_dns( 77,  88,   8,   8);
IPAddress secondary_dns( 77,  88,   8,   1);

SemaphoreHandle_t wifi_lock = NULL;
WiFiServer streamer(WIFI_DATA_PORT);
WiFiServer manager(WIFI_CTRL_PORT);
WiFiClient tuner;

volatile uint8_t b, c;
char i2c1[I2C1_BUFFER_SIZE], i2c2[I2C2_BUFFER_SIZE];
volatile size_t i2c1_len = 0, i2c2_len = 0;

char engine_id = '1';

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
	if (cmd_len == 4) {
		if (strncmp(cmd, "I2C1", 4) == 0) {
			if (i2c1_allowed_to_run != enable) {
				if ((enable > i2c1_allowed_to_run) && (i2c1_has_stopped == true)) {
					i2c1_has_stopped = false;
					i2c1_allowed_to_run = enable;
					start_i2c1_loop();
				} else {
					i2c1_allowed_to_run = enable;
				}
			}
			return;
		} else if (strncmp(cmd, "I2C2", 4) == 0) {
			if (i2c2_allowed_to_run != enable) {
				if ((enable > i2c2_allowed_to_run) && (i2c2_has_stopped == true)) {
					i2c2_has_stopped = false;
					i2c2_allowed_to_run = enable;
					start_i2c2_loop();
				} else {
					i2c2_allowed_to_run = enable;
				}
			}
			return;
		} else if (strncmp(cmd, "UART", 4) == 0) {
			if (uart_allowed_to_run != enable) {
				if ((enable > uart_allowed_to_run) && (uart_has_stopped == true)) {
					uart_has_stopped = false;
					uart_allowed_to_run = enable;
					start_uart_loop();
				} else {
					uart_allowed_to_run = enable;
				}
			}
			return;
		} else if (strncmp(cmd, "BEAT", 4) == 0) {
			if (beat_allowed_to_run != enable) {
				if ((enable > beat_allowed_to_run) && (beat_has_stopped == true)) {
					beat_has_stopped = false;
					beat_allowed_to_run = enable;
					start_beat_loop();
				} else {
					beat_allowed_to_run = enable;
				}
			}
			return;
		}
	}
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
			uart_circle = tmp;
			uart_circle[uart_circle_len].data = (char *)malloc(sizeof(char) * (cmd_len + 1));
			memcpy(uart_circle[uart_circle_len].data, cmd, cmd_len);
			uart_circle[uart_circle_len].data[cmd_len] = '\0';
			uart_circle[uart_circle_len].data_len = cmd_len;
			uart_circle[uart_circle_len].enabled = enable;
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
			uart_circle_pos += 1;
			if (uart_circle_pos >= uart_circle_len) {
				uart_circle_pos = 0;
			}
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
		WiFiClient listener = streamer.available();
		if (listener) listener.write(data, data_len);
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
	size_t packet_ends;
	unsigned long packet_birth;
	while (uart_allowed_to_run == true) {
		if (try_to_write_next_command_from_fast_queue_to_serial() == false) {
			if (try_to_write_next_command_from_uart_circle_to_serial() == false) {
				TASK_DELAY_MS(50);
				continue;
			}
		}
		packet_ends = 0;
		packet_birth = millis();
		uart_len = snprintf(uart, UART_BUFFER_SIZE, "\nUART@%lu=", packet_birth);
		while (true) {
			if (Serial1.available() > 0) {
				if (uart_len < UART_BUFFER_SIZE) {
					uart[uart_len++] = Serial1.read();
					if (uart[uart_len - 1] == '\r') {
						uart[uart_len - 1] = '~';
						packet_ends += 1;
						if (packet_ends > 1) {
							send_data(uart, uart_len);
							break;
						}
					}
				} else {
					Serial1.read();
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
		int beat_len = sprintf(beat_buf, "\nBEAT@%lu=%u", millis(), i);
		if (beat_len > 10) {
			send_data(beat_buf, beat_len);
		}
		i = (i * 10) % 10000;
		TASK_DELAY_MS(1000);
	}
	beat_has_stopped = true;
	vTaskDelete(NULL);
}

void IRAM_ATTR
tuner_loop(void *dummy)
{
#define CMD_SIZE 100
	char cmd[CMD_SIZE];
	uint8_t cmd_len;
	while (1) {
		if (tuner.connected()) {
			if (tuner.available() > 0) {
				cmd_len = 0;
				do {
					cmd[cmd_len++] = tuner.read();
				} while ((tuner.available() > 0) && (cmd_len < CMD_SIZE));
				if (cmd_len > 1) {
					if (cmd[0] == '1') { // send command once
						add_command_to_fast_queue(cmd, cmd_len);
					} else if ((cmd[0] == '+') || (cmd[0] == '-')) { // toggle task/command
						bool enable = cmd[0] == '+' ? true : false;
#define INSTRUCTION_SIZE 10
						char instruction[10];
						uint8_t instruction_len = 0;
						for (uint8_t i = 1; i < cmd_len; ++i) {
							if (cmd[i] == ',') {
								add_command_to_uart_circle(instruction, instruction_len, enable);
								instruction_len = 0;
							} else if (instruction_len < INSTRUCTION_SIZE) {
								instruction[instruction_len++] = cmd[i];
							}
						}
						if (instruction_len != 0) {
							add_command_to_uart_circle(instruction, instruction_len, enable);
						}
					} else if (cmd[0] == 'n') { // set default engine id
						engine_id = cmd[1];
					}
				}
			}
		} else {
			tuner = manager.available();
		}
		TASK_DELAY_MS(100);
	}
	vTaskDelete(NULL);
}

void IRAM_ATTR
pwm_monitor_loop(void *dummy)
{
#define PWMS_BUFFER_SIZE 500
	char pwms[PWMS_BUFFER_SIZE];
	size_t pwms_len;
	while (true) {
		// pulseIn returns microseconds!
		unsigned long packet_birth = millis();
		unsigned long pwm1_high = pulseIn(PWM_IN_1_PIN, HIGH);
		unsigned long pwm2_high = pulseIn(PWM_IN_2_PIN, HIGH);
		unsigned long pwm1_low = pulseIn(PWM_IN_1_PIN, LOW);
		unsigned long pwm2_low = pulseIn(PWM_IN_2_PIN, LOW);
		double pwm1_freq = 1000000.0 / (pwm1_high + pwm1_low);
		double pwm2_freq = 1000000.0 / (pwm2_high + pwm2_low);
		double pwm1_duty = (double)pwm1_high / (pwm1_high + pwm1_low);
		double pwm2_duty = (double)pwm2_high / (pwm2_high + pwm2_low);
		pwms_len = snprintf(pwms, PWMS_BUFFER_SIZE,
			"\nPWMRC@%lu=%.1lf,%.1lf,%.1lf,%.1lf",
			packet_birth,
			pwm1_freq,
			pwm1_duty,
			pwm2_freq,
			pwm2_duty
		);
		if (pwms_len < PWMS_BUFFER_SIZE) send_data(pwms, pwms_len);
		TASK_DELAY_MS(1000);
	}
	vTaskDelete(NULL);
}

void IRAM_ATTR
tachometer_faker_loop(void *dummy)
{
	ledc_timer_config_t timer_config = {
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.duty_resolution = LEDC_TIMER_8_BIT,
		.timer_num = LEDC_TIMER_0,
		.freq_hz = 23,
		.clk_cfg = LEDC_USE_REF_TICK, // 1 MHz, high speed, frequency scaling
		// .deconfigure = false,
	};
	ledc_channel_config_t channel_config = {
		.gpio_num = PWM_OUT_1_PIN,
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.channel = LEDC_CHANNEL_0,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = LEDC_TIMER_0,
		.duty = 127,
		.hpoint = 0,
	};
	ledc_timer_config(&timer_config);
	ledc_channel_config(&channel_config);
	int i = 10;
	while (true) {
		TASK_DELAY_MS(500);
		i = (i + 10) % 1000 + 10;
		ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, i);
	}
	// timer_config.deconfigure = true;
	// ledc_timer_config(&timer_config);
	ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
	vTaskDelete(NULL);
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
start_tuner_loop(void)
{
	xTaskCreatePinnedToCore(
		&tuner_loop,
		"tuner_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_pwm_monitor_loop(void)
{
	xTaskCreatePinnedToCore(
		&pwm_monitor_loop,
		"pwm_monitor_loop",
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
setup(void)
{
	uint64_t input_pins_mask = 0;
	input_pins_mask |= 1ULL << SDA_PIN;
	input_pins_mask |= 1ULL << SCL_PIN;
	input_pins_mask |= 1ULL << SDA2_PIN;
	input_pins_mask |= 1ULL << SCL2_PIN;
	input_pins_mask |= 1ULL << PWM_IN_1_PIN;
	input_pins_mask |= 1ULL << PWM_IN_2_PIN;
	gpio_config_t cfg = {
		input_pins_mask,
		GPIO_MODE_INPUT,
		GPIO_PULLUP_DISABLE,
		GPIO_PULLDOWN_DISABLE,
		GPIO_INTR_DISABLE,
	};
	gpio_config(&cfg);
	wifi_lock = xSemaphoreCreateMutex();
	fast_queue_lock = xSemaphoreCreateMutex();
	uart_circle_lock = xSemaphoreCreateMutex();
	Serial1.begin(SERIAL_SPEED, SERIAL_8N1, RX_PIN, TX_PIN);
	add_command_to_uart_circle("RAC", 3, true);
	add_command_to_uart_circle("RFI", 3, true);
	WiFi.config(ip, gateway, subnet, primary_dns, secondary_dns);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
	}
	ArduinoOTA.begin();
	streamer.begin();
	manager.begin();
	start_i2c1_loop();
	start_i2c2_loop();
	start_uart_loop();
	// start_beat_loop();
	start_tuner_loop();
	start_pwm_monitor_loop();
	start_tachometer_faker_loop();
}

void
loop(void)
{
	if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
		ArduinoOTA.handle();
		xSemaphoreGive(wifi_lock);
	}
	delay(1000);
}
