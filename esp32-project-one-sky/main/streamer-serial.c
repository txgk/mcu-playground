#include "main.h"

static void
serial_streamer(void *dummy)
{
	while (true) {
		TASK_DELAY_MS(1000);
	}
	vTaskDelete(NULL);
}

bool
start_serial_streamer(void)
{
	xTaskCreatePinnedToCore(&serial_streamer, "serial_streamer", 4096, NULL, 1, NULL, 1);
	return true;
}
