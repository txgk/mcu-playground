#include <stdio.h>
#include "nosedive.h"

#define SPEED_SAMPLES_COUNT 64
#define SPEED_MESSAGE_SIZE 100

void IRAM_ATTR
speed_task(void *dummy)
{
	char speed_text_buf[SPEED_MESSAGE_SIZE];
	int speed_text_len;
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		uint64_t read_sum = 0;
		for (uint8_t i = 0; i < SPEED_SAMPLES_COUNT; ++i) {
			int read_sample = 0;
			if (adc_oneshot_read(adc1_handle, SPEED_ADC_CHANNEL, &read_sample) == ESP_OK) {
				read_sum += read_sample;
			}
		}
		speed_text_len = snprintf(
			speed_text_buf,
			SPEED_MESSAGE_SIZE,
			"SPEED@%lld=%llu\n",
			ms,
			read_sum / SPEED_SAMPLES_COUNT
		);
		if (speed_text_len > 0 && speed_text_len < SPEED_MESSAGE_SIZE) {
			send_data(speed_text_buf, speed_text_len);
			uart_write_bytes(UART_NUM_0, speed_text_buf, speed_text_len);
		}
		TASK_DELAY_MS(SPEED_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
