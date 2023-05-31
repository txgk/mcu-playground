#include <stdio.h>
#include "nosedive.h"
#include "driver-lis3dh.h"

#define LIS3DH_MESSAGE_SIZE 200

static char lis3dh_text_buf[LIS3DH_MESSAGE_SIZE];
static int lis3dh_text_len;

void IRAM_ATTR
lis3dh_task(void *dummy)
{
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int id = 0;
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			id = lis3dh_read_device_id();
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
		lis3dh_text_len = snprintf(
			lis3dh_text_buf,
			LIS3DH_MESSAGE_SIZE,
			"LIS3DH@%lld=%d\n",
			ms,
			id
		);
		if (lis3dh_text_len > 0 && lis3dh_text_len < LIS3DH_MESSAGE_SIZE) {
			send_data(lis3dh_text_buf, lis3dh_text_len);
			uart_write_bytes(UART_NUM_0, lis3dh_text_buf, lis3dh_text_len);
		}
		TASK_DELAY_MS(LIS3DH_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
