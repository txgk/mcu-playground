#include "nosedive.h"

#define POWER_MESSAGE_SIZE 100

void IRAM_ATTR
power_task(void *dummy)
{
	char power_text_buf[POWER_MESSAGE_SIZE];
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int power_text_len = snprintf(
			power_text_buf,
			POWER_MESSAGE_SIZE,
			"POWER@%lld=%d,%d,%d\n",
			ms,
			gpio_get_level(KILL_SWITCH_PIN),
			gpio_get_level(POWER_ON_PIN),
			gpio_get_level(CURRENT_ALERT_PIN)
		);
		if (power_text_len > 0 && power_text_len < POWER_MESSAGE_SIZE) {
			send_data(power_text_buf, power_text_len);
			uart_write_bytes(UART_NUM_0, power_text_buf, power_text_len);
		}
		TASK_DELAY_MS(POWER_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
