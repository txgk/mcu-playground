#include "nosedive.h"
#include "driver-tpa626.h"

#define TPA626_MESSAGE_SIZE 200

void IRAM_ATTR
tpa626_task(void *dummy)
{
	char tpa626_text_buf[TPA626_MESSAGE_SIZE];
	if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
		if (tpa626_initialize() == false) {
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
			vTaskDelete(NULL);
		}
		xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
	} else {
		vTaskDelete(NULL);
	}
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		double shunt_voltage = 0, bus_voltage = 0, current = 0, power = 0;
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			shunt_voltage = tpa626_read_shunt_voltage();
			bus_voltage = tpa626_read_bus_voltage();
			current = tpa626_read_current();
			power = tpa626_read_power();
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
		int tpa626_text_len = snprintf(
			tpa626_text_buf,
			TPA626_MESSAGE_SIZE,
			"TPA626@%lld=%.2lf,%.2lf,%.6lf,%.4lf\n",
			ms,
			shunt_voltage,
			bus_voltage,
			current,
			power
		);
		if (tpa626_text_len > 0 && tpa626_text_len < TPA626_MESSAGE_SIZE) {
			send_data(tpa626_text_buf, tpa626_text_len);
			uart_write_bytes(UART_NUM_0, tpa626_text_buf, tpa626_text_len);
		}
		TASK_DELAY_MS(TPA626_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
