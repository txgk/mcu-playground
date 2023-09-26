#include "main.h"

static void
serial_streamer(void *dummy)
{
	while (true) {
		for (size_t i = 0; tasks[i].prefix != NULL; ++i) {
			int64_t current_time_ms = esp_timer_get_time() / 1000;
			if (tasks[i].last_inform_ms[1] + tasks[i].informer_period_ms < current_time_ms) {
				tasks[i].last_inform_ms[1] = current_time_ms;
				int data_len = tasks[i].informer(&tasks[i], tasks[i].informer_data[1]);
				if (data_len > 0) {
					uart_write_bytes(UART0_PORT, tasks[i].informer_data[1], data_len);
				}
			}
		}
		TASK_DELAY_MS(100);
	}
	vTaskDelete(NULL);
}

bool
start_serial_streamer(void)
{
	xTaskCreatePinnedToCore(&serial_streamer, "serial_streamer", 4096, NULL, 1, NULL, 1);
	return true;
}
