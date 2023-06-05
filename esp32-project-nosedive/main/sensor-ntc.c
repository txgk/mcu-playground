#include <stdio.h>
#include "nosedive.h"

#define NTC_SAMPLES_COUNT 64
#define NTC_MESSAGE_SIZE 100

static char ntc_text_buf[NTC_MESSAGE_SIZE];
static int ntc_text_len;

void IRAM_ATTR
ntc_task(void *dummy)
{
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		uint64_t read_sum = 0;
		for (uint8_t i = 0; i < NTC_SAMPLES_COUNT; ++i) {
			int read_sample = 0;
			if (adc_oneshot_read(adc1_handle, NTC_ADC_CHANNEL, &read_sample) == ESP_OK) {
				read_sum += read_sample;
			}
		}
		ntc_text_len = snprintf(
			ntc_text_buf,
			NTC_MESSAGE_SIZE,
			"NTC@%lld=%llu\n",
			ms,
			read_sum / NTC_SAMPLES_COUNT
		);
		if (ntc_text_len > 0 && ntc_text_len < NTC_MESSAGE_SIZE) {
			send_data(ntc_text_buf, ntc_text_len);
			uart_write_bytes(UART_NUM_0, ntc_text_buf, ntc_text_len);
		}
		TASK_DELAY_MS(NTC_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
