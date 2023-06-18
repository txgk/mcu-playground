#include "nosedive.h"

#define DRIVER_SPEED_SAMPLES_COUNT 64

static volatile int64_t  speed_measurement_time  = 0;
static volatile uint64_t speed_measurement_value = 0;

void IRAM_ATTR
speed_task(void *arg)
{
	struct task_descriptor *task = arg;
	while (true) {
		if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
			speed_measurement_time = esp_timer_get_time();
			speed_measurement_value = 0;
			for (uint8_t i = 0; i < DRIVER_SPEED_SAMPLES_COUNT; ++i) {
				int read_sample = 0;
				if (adc_oneshot_read(adc1_handle, SPEED_ADC_CHANNEL, &read_sample) == ESP_OK) {
					speed_measurement_value += read_sample;
				}
			}
			speed_measurement_value /= DRIVER_SPEED_SAMPLES_COUNT;
			xSemaphoreGive(task->mutex);
		}
		TASK_DELAY_MS(task->performer_period_ms);
	}
	vTaskDelete(NULL);
}

int
speed_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%llu\n",
			task->prefix,
			speed_measurement_time / 1000,
			speed_measurement_value
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
