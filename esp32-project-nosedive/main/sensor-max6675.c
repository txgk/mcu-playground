#include <stdio.h>
#include "nosedive.h"
#include "driver-max6675.h"

#define MAX6675_MESSAGE_SIZE 200

static char max6675_text_buf[MAX6675_MESSAGE_SIZE];
static int max6675_text_len;

void IRAM_ATTR
max6675_task(void *dummy)
{
	if (max6675_initialize() == false) {
		vTaskDelete(NULL);
	}
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int temp = max6675_read_temperature();
		max6675_text_len = snprintf(
			max6675_text_buf,
			MAX6675_MESSAGE_SIZE,
			"MAX6675@%lld=%d\n",
			ms,
			temp
		);
		if (max6675_text_len > 0 && max6675_text_len < MAX6675_MESSAGE_SIZE) {
			send_data(max6675_text_buf, max6675_text_len);
			uart_write_bytes(UART_NUM_0, max6675_text_buf, max6675_text_len);
		}
		TASK_DELAY_MS(MAX6675_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
