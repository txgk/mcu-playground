#include <stdio.h>
#include "nosedive.h"
#include "driver-w25q64.h"

#define WINBOND_MESSAGE_SIZE 200

static char winbond_text_buf[WINBOND_MESSAGE_SIZE];
static int winbond_text_len;

void IRAM_ATTR
winbond_task(void *dummy)
{
	if (winbond_initialize() == false) {
		vTaskDelete(NULL);
	}
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		uint8_t data[3];
		W25Q64_readManufacturer(data);
		winbond_text_len = snprintf(
			winbond_text_buf,
			WINBOND_MESSAGE_SIZE,
			"WINBOND@%lld=%d,%d,%d\n",
			ms,
			data[0],
			data[1],
			data[2]
		);
		if (winbond_text_len > 0 && winbond_text_len < WINBOND_MESSAGE_SIZE) {
			send_data(winbond_text_buf, winbond_text_len);
			uart_write_bytes(UART_NUM_0, winbond_text_buf, winbond_text_len);
		}
		TASK_DELAY_MS(1000);
	}
	vTaskDelete(NULL);
}
