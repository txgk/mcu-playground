#include "nosedive.h"
#include "driver-pca9685.h"

const struct data_piece *
pca9685_http_handler_pcaset(const char *value)
{
	long ch = 0, duty = 100;
	uint8_t field = 0;
	char buf[10];
	uint8_t buf_len = 0;
	for (const char *i = value; ; ++i) {
		if (*i == ',' || *i == '\0') {
			buf[buf_len] = '\0';
			buf_len = 0;
			field += 1;
			if (field == 1) {
				if (strcmp(buf, "all") == 0) {
					ch = PCA9685_ALL;
				} else {
					ch = strtol(buf, NULL, 10);
				}
			} else if (field == 2) {
				duty = strtol(buf, NULL, 10);
			}
			if (*i == '\0') {
				break;
			}
		} else if (buf_len < 9) {
			buf[buf_len++] = *i;
		}
	}
	if (field > 0) {
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			pca9685_channel_setup(PCA9685_CH0 + ch, duty, 0);
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
	}
	return NULL;
}

static void
pca9685_http_handler_pcamax_pcaoff(const char *value, bool on)
{
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		if (strcmp(value, "all") == 0) {
			pca9685_channel_full_toggle(PCA9685_ALL, on);
		} else {
			long ch = strtol(value, NULL, 10);
			pca9685_channel_full_toggle(PCA9685_CH0 + ch, on);
		}
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
}

const struct data_piece *
pca9685_http_handler_pcamax(const char *value)
{
	pca9685_http_handler_pcamax_pcaoff(value, true);
	return NULL;
}

const struct data_piece *
pca9685_http_handler_pcaoff(const char *value)
{
	pca9685_http_handler_pcamax_pcaoff(value, false);
	return NULL;
}

const struct data_piece *
pca9685_http_handler_pcafreq(const char *value)
{
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		long freq = strtol(value, NULL, 10);
		pca9685_change_frequency(freq);
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	return NULL;
}

void
pca9685_print_some_registers(void)
{
#define PCA9685_MESSAGE_SIZE 100
	char pca9685_text_buf[PCA9685_MESSAGE_SIZE];
	int mode1 = 0, subadr1 = 0;
	int64_t ms = esp_timer_get_time() / 1000;
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		mode1 = pca9685_read_mode1();
		subadr1 = pca9685_read_subadr1();
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	int pca9685_text_len = snprintf(
		pca9685_text_buf,
		PCA9685_MESSAGE_SIZE,
		"PCA9685@%lld=%d,%d\n",
		ms,
		mode1,
		subadr1
	);
	if (pca9685_text_len > 0 && pca9685_text_len < PCA9685_MESSAGE_SIZE) {
		send_data(pca9685_text_buf, pca9685_text_len);
		uart_write_bytes(UART_NUM_0, pca9685_text_buf, pca9685_text_len);
	}
}
