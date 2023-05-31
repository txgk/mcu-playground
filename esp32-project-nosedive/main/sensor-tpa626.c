#include <stdio.h>
#include "nosedive.h"
#include "driver-tpa626.h"

#define TPA626_MESSAGE_SIZE 200

static char tpa626_text_buf[TPA626_MESSAGE_SIZE];
static int tpa626_text_len;

void IRAM_ATTR
tpa626_task(void *dummy)
{
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int cfg = 0;
		if (xSemaphoreTake(system_mutexes[MUX_I2C_DRIVER], portMAX_DELAY) == pdTRUE) {
			cfg = tpa626_read_configuration();
			xSemaphoreGive(system_mutexes[MUX_I2C_DRIVER]);
		}
		tpa626_text_len = snprintf(
			tpa626_text_buf,
			TPA626_MESSAGE_SIZE,
			"TPA626@%lld=%d\n",
			ms,
			cfg
		);
		if (tpa626_text_len > 0 && tpa626_text_len < TPA626_MESSAGE_SIZE) {
			send_data(tpa626_text_buf, tpa626_text_len);
			uart_write_bytes(UART_NUM_0, tpa626_text_buf, tpa626_text_len);
		}
		TASK_DELAY_MS(TPA626_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
