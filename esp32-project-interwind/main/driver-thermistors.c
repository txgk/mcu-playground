#include "interwind.h"

#define SAMPLES_COUNT_PER_THERMISTOR_MEASUREMENT 8

struct thermistor_billet {
	const adc_channel_t ch;
	int measurement;
	int64_t time_mark;
};

static volatile struct thermistor_billet thermistor_sets[] = {
	{THERMISTOR_0_ADC_CH, 0, 0},
	{THERMISTOR_1_ADC_CH, 0, 0},
	{THERMISTOR_2_ADC_CH, 0, 0},
	{THERMISTOR_3_ADC_CH, 0, 0},
	{THERMISTOR_4_ADC_CH, 0, 0},
	{THERMISTOR_5_ADC_CH, 0, 0},
	{THERMISTOR_6_ADC_CH, 0, 0},
	{THERMISTOR_7_ADC_CH, 0, 0},
};

void IRAM_ATTR
therm_task(void *arg)
{
	struct task_descriptor *task = arg;
	while (true) {
		if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
			for (uint8_t i = 0; i < LENGTH(thermistor_sets); ++i) {
				thermistor_sets[i].measurement = 0;
				thermistor_sets[i].time_mark = esp_timer_get_time();
				for (uint8_t j = 0; j < SAMPLES_COUNT_PER_THERMISTOR_MEASUREMENT; ++j) {
					int read_sample = 0;
					if (adc_oneshot_read(adc1_handle, thermistor_sets[i].ch, &read_sample) == ESP_OK) {
						thermistor_sets[i].measurement += read_sample;
					}
				}
				thermistor_sets[i].measurement /= SAMPLES_COUNT_PER_THERMISTOR_MEASUREMENT;
			}
			xSemaphoreGive(task->mutex);
		}
		TASK_DELAY_MS(task->performer_period_ms);
	}
	vTaskDelete(NULL);
}

int
therm_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"THERM0@%lld=%d\n"
			"THERM1@%lld=%d\n"
			"THERM2@%lld=%d\n"
			"THERM3@%lld=%d\n"
			"THERM4@%lld=%d\n"
			"THERM5@%lld=%d\n"
			"THERM6@%lld=%d\n"
			"THERM7@%lld=%d\n",
			thermistor_sets[0].time_mark / 1000, thermistor_sets[0].measurement,
			thermistor_sets[1].time_mark / 1000, thermistor_sets[1].measurement,
			thermistor_sets[2].time_mark / 1000, thermistor_sets[2].measurement,
			thermistor_sets[3].time_mark / 1000, thermistor_sets[3].measurement,
			thermistor_sets[4].time_mark / 1000, thermistor_sets[4].measurement,
			thermistor_sets[5].time_mark / 1000, thermistor_sets[5].measurement,
			thermistor_sets[6].time_mark / 1000, thermistor_sets[6].measurement,
			thermistor_sets[7].time_mark / 1000, thermistor_sets[7].measurement
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
