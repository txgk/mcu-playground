#include <stdio.h>
#include "nosedive.h"
#include "driver-bmx280.h"

#define BMX280_MESSAGE_SIZE 200

static char bmx280_text_buf[BMX280_MESSAGE_SIZE];
static int bmx280_text_len;

void IRAM_ATTR
bmx280_task(void *dummy)
{
	// It just allocates memory, so wrapping it into I2C driver mutex is not needed.
	bmx280_t *bmx280 = bmx280_create(I2C_NUM_0);
	if (bmx280 == NULL) {
		vTaskDelete(NULL);
	}
	bmx280_config_t bmx_cfg = BMX280_DEFAULT_CONFIG;
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		bmx280_init(bmx280);
		bmx280_configure(bmx280, &bmx_cfg);
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	} else {
		vTaskDelete(NULL);
	}
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		float temp = 0, pres = 0, hum = 0;
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			bmx280_setMode(bmx280, BMX280_MODE_FORCE);
			do {
				TASK_DELAY_MS(50);
			} while(bmx280_isSampling(bmx280));
			bmx280_readoutFloat(bmx280, &temp, &pres, &hum);
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
		bmx280_text_len = snprintf(
			bmx280_text_buf,
			BMX280_MESSAGE_SIZE,
			"BME280@%lld=%.2f,%.2f,%.2f\n",
			ms,
			temp,
			pres,
			hum
		);
		if (bmx280_text_len > 0 && bmx280_text_len < BMX280_MESSAGE_SIZE) {
			send_data(bmx280_text_buf, bmx280_text_len);
			uart_write_bytes(UART_NUM_0, bmx280_text_buf, bmx280_text_len);
		}
		TASK_DELAY_MS(BMX280_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
