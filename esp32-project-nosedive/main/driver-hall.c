#include "nosedive.h"
#include "driver-hall.h"

#define HALL_SAMPLES_COUNT         100
#define HALL_MEASUREMENT_PERIOD_US 5000000

static volatile _Atomic int64_t hall_samples[HALL_SAMPLES_COUNT];
static volatile _Atomic int64_t last_sample_time = 0;
static volatile _Atomic size_t hall_iter = 0;
static volatile _Atomic bool hall_in_use = false;
static volatile double hall_freq         = 0;

void IRAM_ATTR
hall_take_sample(void *dummy)
{
	hall_in_use = true;
	last_sample_time = esp_timer_get_time();
	hall_samples[hall_iter] = last_sample_time;
	hall_iter = (hall_iter + 1) % HALL_SAMPLES_COUNT;
	size_t fresh_samples_count = 0;
	int64_t oldest_sample_time = last_sample_time;
	for (size_t i = 0; i < HALL_SAMPLES_COUNT; ++i) {
		if (last_sample_time - hall_samples[i] < HALL_MEASUREMENT_PERIOD_US) {
			fresh_samples_count += 1;
			if (hall_samples[i] < oldest_sample_time) {
				oldest_sample_time = hall_samples[i];
			}
		}
	}
	// Частота = множитель мкс * кол-во измерений за период / период съёма измерений в мкс
	hall_freq = 1000000.0 * fresh_samples_count / (last_sample_time - oldest_sample_time);
	hall_in_use = false;
}

void IRAM_ATTR
hall_task(void *arg)
{
	struct task_descriptor *task = arg;
	while (true) {
		if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
			while (hall_in_use == true);
			if ((esp_timer_get_time() - last_sample_time > HALL_MEASUREMENT_PERIOD_US) && (hall_in_use == false)) {
				last_sample_time = esp_timer_get_time();
				hall_freq = 0;
			}
			xSemaphoreGive(task->mutex);
		}
		TASK_DELAY_MS(task->performer_period_ms);
	}
	vTaskDelete(NULL);
}

int
hall_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		while (hall_in_use == true);
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%.2lf\n",
			task->prefix,
			last_sample_time / 1000,
			hall_freq * 60 // Convert to RPM.
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
