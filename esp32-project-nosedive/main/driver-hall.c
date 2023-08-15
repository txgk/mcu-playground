#include "nosedive.h"
#include "driver-hall.h"

#define HALL_SAMPLES_COUNT         100
#define HALL_MEASUREMENT_PERIOD_US 5000000

static volatile SemaphoreHandle_t hall_binary_semaphore = NULL;
static volatile int64_t hall_samples[HALL_SAMPLES_COUNT];
static volatile int64_t last_sample_time = 0;
static volatile size_t hall_iter = 0;

void IRAM_ATTR
hall_take_sample(void *dummy)
{
	if (hall_binary_semaphore != NULL) {
		xSemaphoreGiveFromISR(hall_binary_semaphore, NULL);
	}
}

void IRAM_ATTR
hall_task(void *arg)
{
	struct task_descriptor *task = arg;
	hall_binary_semaphore = xSemaphoreCreateBinary();
	if (hall_binary_semaphore != NULL) {
		while (true) {
			if (xSemaphoreTake(hall_binary_semaphore, portMAX_DELAY) == pdTRUE) {
				if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
					last_sample_time = esp_timer_get_time();
					hall_samples[hall_iter] = last_sample_time;
					hall_iter = (hall_iter + 1) % HALL_SAMPLES_COUNT;
					xSemaphoreGive(task->mutex);
				}
			}
		}
	}
	vTaskDelete(NULL);
}

int
hall_info(struct task_descriptor *task, char *dest)
{
	const int64_t current_time = esp_timer_get_time();
	double hall_freq = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		if (current_time - last_sample_time <= HALL_MEASUREMENT_PERIOD_US) {
			size_t fresh_samples_count = 0;
			int64_t oldest_sample_time = last_sample_time;
			for (size_t i = 0; i < HALL_SAMPLES_COUNT; ++i) {
				if (current_time - hall_samples[i] <= HALL_MEASUREMENT_PERIOD_US) {
					fresh_samples_count += 1;
					if (hall_samples[i] < oldest_sample_time) {
						oldest_sample_time = hall_samples[i];
					}
				}
			}
			if (last_sample_time > oldest_sample_time) {
				// Частота = множитель мкс * кол-во измерений за период / период съёма измерений в мкс
				hall_freq = 1000000.0 * fresh_samples_count / (last_sample_time - oldest_sample_time);
			}
		}
		xSemaphoreGive(task->mutex);
	}
	int len = 0;
	len = snprintf(dest, MESSAGE_SIZE_LIMIT,
		"%s@%lld=%.2lf\n",
		task->prefix,
		current_time / 1000,
		hall_freq * 60 // Convert to RPM.
	);
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
