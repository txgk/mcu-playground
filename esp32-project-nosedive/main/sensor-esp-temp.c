#include <stdio.h>
#include "nosedive.h"

// Undocumented secret function...
uint8_t temprature_sens_read();

void IRAM_ATTR
esp_temp_task(void *dummy)
{
#define ESP_TEMP_MESSAGE_SIZE 100
	char esp_temp_text_buf[ESP_TEMP_MESSAGE_SIZE];
	int esp_temp_text_len;
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int temp = ((int)temprature_sens_read() - 32) * 5 / 9;
		esp_temp_text_len = snprintf(
			esp_temp_text_buf,
			ESP_TEMP_MESSAGE_SIZE,
			"ESPTEMP@%lld=%d\n",
			ms,
			temp
		);
		if (esp_temp_text_len > 0 && esp_temp_text_len < ESP_TEMP_MESSAGE_SIZE) {
			send_data(esp_temp_text_buf, esp_temp_text_len);
			uart_write_bytes(UART_NUM_0, esp_temp_text_buf, esp_temp_text_len);
		}
		TASK_DELAY_MS(ESP_TEMP_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
