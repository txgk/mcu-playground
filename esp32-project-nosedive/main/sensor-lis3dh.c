#include "nosedive.h"
#include "driver-lis3dh.h"

#define LIS3DH_MESSAGE_SIZE 200

void IRAM_ATTR
lis3dh_task(void *dummy)
{
	char lis3dh_text_buf[LIS3DH_MESSAGE_SIZE];
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		lis3dh_initialize();
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	}
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int id = 0;
		uint16_t x, y, z;
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			id = lis3dh_read_device_id();
			lis3dh_read_acceleration(&x, &y, &z);
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
		int lis3dh_text_len = snprintf(
			lis3dh_text_buf,
			LIS3DH_MESSAGE_SIZE,
			"LIS3DH@%lld=%d,%d,%d,%d\n",
			ms,
			id,
			x,
			y,
			z
		);
		if (lis3dh_text_len > 0 && lis3dh_text_len < LIS3DH_MESSAGE_SIZE) {
			send_data(lis3dh_text_buf, lis3dh_text_len);
			uart_write_bytes(UART_NUM_0, lis3dh_text_buf, lis3dh_text_len);
		}
		TASK_DELAY_MS(LIS3DH_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
