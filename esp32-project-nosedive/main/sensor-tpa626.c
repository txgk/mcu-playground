#include <stdio.h>
#include "nosedive.h"
#include "tpa626.h"

static char tpa626_text_buf[200];
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
			200,
			"TPA626@%lld=%d\n",
			ms,
			cfg
		);
		if (tpa626_text_len > 0 && tpa626_text_len < 200) {
			send_data(tpa626_text_buf, tpa626_text_len);
			uart_write_bytes(UART_NUM_0, tpa626_text_buf, tpa626_text_len);
		}
		TASK_DELAY_MS(TPA626_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
