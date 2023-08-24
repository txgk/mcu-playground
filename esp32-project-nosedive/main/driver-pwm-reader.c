#include "nosedive.h"

#define FADE_OUT_PERIOD_US 1000000

struct pwm_reader_designation {
	const gpio_num_t gpio;
	int freq;
	int duty;
	SemaphoreHandle_t bin_semaph;
	int previous_level;
	int64_t period;
	int64_t high_time;
	int64_t rising_moment;
	int64_t last_time;
};

static volatile struct pwm_reader_designation pwm_sets[] = {
	{PWM1_INPUT_PIN, 0.0, 0.0, NULL, 0, 0, 0, 0, 0},
	{PWM2_INPUT_PIN, 0.0, 0.0, NULL, 0, 0, 0, 0, 0},
};

void IRAM_ATTR
trigger_pwm_calculation(void *pwm_set_id)
{
	if (pwm_sets[(int)pwm_set_id].bin_semaph != NULL) {
		xSemaphoreGiveFromISR(pwm_sets[(int)pwm_set_id].bin_semaph, NULL);
	}
}

void IRAM_ATTR
pwm_reader_task(void *arg)
{
	struct task_descriptor *task = arg;
	volatile struct pwm_reader_designation *pwm_set = NULL;
	if (strcmp(task->prefix, "PWM1") == 0) {
		pwm_set = pwm_sets + 0;
	} else if (strcmp(task->prefix, "PWM2") == 0) {
		pwm_set = pwm_sets + 1;
	} else {
		vTaskDelete(NULL);
		return; // Бесполезный return, чтобы компилятор меньше ныл.
	}
	pwm_set->bin_semaph = xSemaphoreCreateBinary();
	if (pwm_set->bin_semaph != NULL) {
		while (true) {
			if (xSemaphoreTake(pwm_set->bin_semaph, portMAX_DELAY) == pdTRUE) {
				const int level = gpio_get_level(pwm_set->gpio);
				if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
					pwm_set->last_time = esp_timer_get_time();
					if (level > 0 && pwm_set->previous_level <= 0) {
						pwm_set->period = pwm_set->last_time - pwm_set->rising_moment;
						pwm_set->rising_moment = pwm_set->last_time;
					} else {
						pwm_set->high_time = pwm_set->last_time - pwm_set->rising_moment;
					}
					pwm_set->previous_level = level;
					pwm_set->freq = pwm_set->period == 0 ? 0 : 1000000 / pwm_set->period;
					pwm_set->duty = pwm_set->period == 0 ? 0 : 100 * pwm_set->high_time / pwm_set->period;
					xSemaphoreGive(task->mutex);
				}
			}
		}
	}
	vTaskDelete(NULL);
}

int
pwm_reader_info(struct task_descriptor *task, char *dest)
{
	volatile struct pwm_reader_designation *pwm_set = NULL;
	if (strcmp(task->prefix, "PWM1") == 0) {
		pwm_set = pwm_sets + 0;
	} else if (strcmp(task->prefix, "PWM2") == 0) {
		pwm_set = pwm_sets + 1;
	} else {
		return 0;
	}
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		if (esp_timer_get_time() - pwm_set->last_time > FADE_OUT_PERIOD_US) {
			pwm_set->freq = 0;
			pwm_set->duty = 0;
		}
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%d,%d\n",
			task->prefix,
			pwm_set->last_time / 1000,
			pwm_set->freq,
			pwm_set->duty
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
