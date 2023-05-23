#include <stdio.h>
#include "nosedive.h"

static char heartbeat_text_buf[100];
static int heartbeat_text_len;

void
heartbeat_task(void *dummy)
{
	int32_t i = 1;
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		heartbeat_text_len = snprintf(heartbeat_text_buf, 100, "BEAT@%lld=%ld\n", ms, i);
		if (heartbeat_text_len > 0 && heartbeat_text_len < 100) {
			send_data(heartbeat_text_buf, heartbeat_text_len);
			uart_write_bytes(UART_NUM_0, heartbeat_text_buf, heartbeat_text_len);
		}
		i = (i * 10) % 999999999;
		TASK_DELAY_MS(1000);
	}
	vTaskDelete(NULL);
}
