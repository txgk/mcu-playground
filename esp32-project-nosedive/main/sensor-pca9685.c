#include <stdio.h>
#include "nosedive.h"
#include "driver-pca9685.h"

#define PCA9685_MESSAGE_SIZE 100
static char pca9685_text_buf[PCA9685_MESSAGE_SIZE];
static int pca9685_text_len;

static void
pca9685_print_some_registers(void)
{
	int mode1 = 0, subadr1 = 0;
	int64_t ms = esp_timer_get_time() / 1000;
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		mode1 = pca9685_read_mode1();
		subadr1 = pca9685_read_subadr1();
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	pca9685_text_len = snprintf(
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

void IRAM_ATTR
pca9685_task(void *dummy)
{
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		pca9685_initialize();
		pca9685_change_frequency(1);
		pca9685_channel_full_toggle(PCA9685_ALL, false);
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	while (true) {
		for (pca9685_ch_t ch = PCA9685_CH6; ch < PCA9685_CH10; ++ch) {
			if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
				pca9685_channel_full_toggle(ch, true);
				pca9685_channel_full_toggle(ch == PCA9685_CH6 ? PCA9685_CH9 : ch - 1, false);
				// pca9685_text_len = sprintf(pca9685_text_buf, "ch is %d\n", ch);
				// uart_write_bytes(UART_NUM_0, pca9685_text_buf, pca9685_text_len);
				xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
			}
			TASK_DELAY_MS(400);
		}
		for (int i = 1; i < 9; ++i) {
			if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
				pca9685_channel_full_toggle(PCA9685_ALL, i % 2);
				xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
			}
			TASK_DELAY_MS(500);
		}
		// pca9685_print_some_registers();
		// TASK_DELAY_MS(PCA9685_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
