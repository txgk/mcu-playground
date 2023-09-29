#include "main.h"
#include "HX711.h"

static void
hx711_task(void *arg)
{
	while(1) {
		unsigned long int weight = HX711_get_units(HX711_SAMPLES_COUNT);
		TASK_DELAY_MS(1000);
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
