#include <stdio.h>
#include "nosedive.h"
#include "driver-hall.h"

#define HALL_MESSAGE_SIZE 100

void IRAM_ATTR
hall_task(void *dummy)
{
	char hall_text_buf[HALL_MESSAGE_SIZE];
	int hall_text_len;
	hall_initialize();
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
#ifdef READ_HALL_AS_ANALOG_PIN
		int read_sample = 0;
		while (adc_oneshot_read(adc2_handle, HALL_ADC_CHANNEL, &read_sample) != ESP_OK) {
			TASK_DELAY_MS(100);
		}
		hall_text_len = snprintf(
			hall_text_buf,
			HALL_MESSAGE_SIZE,
			"HALL@%lld=%d\n",
			ms,
			read_sample
		);
#else
		hall_text_len = snprintf(
			hall_text_buf,
			HALL_MESSAGE_SIZE,
			"HALL@%lld=%d\n",
			ms,
			gpio_get_level(HALL_PIN)
		);
#endif
		// if (hall_intr_triggered == true) {
		// 	hall_intr_triggered = false;
		// 	hall_text_len = snprintf(
		// 		hall_text_buf,
		// 		HALL_MESSAGE_SIZE,
		// 		"HALL@%lld=YYYYYYYYESSSSSES\n",
		// 		ms
		// 	);
		// } else {
		// 	hall_text_len = snprintf(
		// 		hall_text_buf,
		// 		HALL_MESSAGE_SIZE,
		// 		"HALL@%lld=NO\n",
		// 		ms
		// 	);
		// }
		if (hall_text_len > 0 && hall_text_len < HALL_MESSAGE_SIZE) {
			send_data(hall_text_buf, hall_text_len);
			uart_write_bytes(UART_NUM_0, hall_text_buf, hall_text_len);
		}
		TASK_DELAY_MS(500);
	}
	vTaskDelete(NULL);
}
