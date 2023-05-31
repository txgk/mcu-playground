#include <stdio.h>
#include "nosedive.h"
#include "driver-pca9685.h"

#define PCA9685_MESSAGE_SIZE 100

static char pca9685_text_buf[PCA9685_MESSAGE_SIZE];
static int pca9685_text_len;

void IRAM_ATTR
pca9685_task(void *dummy)
{
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int cfg = 0;
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			cfg = pca9685_read_subadr1();
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
		pca9685_text_len = snprintf(
			pca9685_text_buf,
			PCA9685_MESSAGE_SIZE,
			"PCA9685@%lld=%d\n",
			ms,
			cfg
		);
		if (pca9685_text_len > 0 && pca9685_text_len < PCA9685_MESSAGE_SIZE) {
			send_data(pca9685_text_buf, pca9685_text_len);
			uart_write_bytes(UART_NUM_0, pca9685_text_buf, pca9685_text_len);
		}
		TASK_DELAY_MS(PCA9685_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
