#include "main.h"
#include "HX711.h"

static void
hx711_task(void *arg)
{
	char out[500];
	while (1) {
		int64_t packet_birth = esp_timer_get_time() / 1000;
		unsigned long int weight = HX711_get_units(HX711_SAMPLES_COUNT);
		int out_len = snprintf(out, 500, "HX711@%lld=%lu\n",
			packet_birth,
			weight
		);
		if (out_len > 0 && out_len < 500) stream_write(out, out_len);
		TASK_DELAY_MS(500);
	}
}

bool
hx711_init(void)
{
	HX711_init(HX711_DATA_PIN, HX711_SCLK_PIN, HX711_MEASURE_GAIN);
	HX711_tare();
	xTaskCreatePinnedToCore(hx711_task, "hx711_task", 4096, NULL, 1, NULL, 0);
	return true;
}
