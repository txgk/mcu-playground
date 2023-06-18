#include "nosedive.h"

#define FADE_OUT_PERIOD_US 1000000

static volatile int64_t pwm_reader_measurement_time = 0;
static volatile double pwm1_frequency               = 0;
static volatile double pwm1_duty_cycle              = 0;
static volatile double pwm2_frequency               = 0;
static volatile double pwm2_duty_cycle              = 0;

static volatile _Atomic int pwm1_previous_level    = 0;
static volatile _Atomic int64_t pwm1_period        = 0;
static volatile _Atomic int64_t pwm1_high_time     = 0;
static volatile _Atomic int64_t pwm1_rising_moment = 0;
static volatile _Atomic int pwm2_previous_level    = 0;
static volatile _Atomic int64_t pwm2_period        = 0;
static volatile _Atomic int64_t pwm2_high_time     = 0;
static volatile _Atomic int64_t pwm2_rising_moment = 0;
static volatile _Atomic int64_t us                 = 0;

void IRAM_ATTR
calculate_pwm(void *gpio_num)
{
	const int level = gpio_get_level((gpio_num_t)gpio_num);
	us = esp_timer_get_time();
	if ((gpio_num_t)gpio_num == PWM1_INPUT_PIN) {
		if (level > 0 && pwm1_previous_level <= 0) {
			pwm1_period = us - pwm1_rising_moment;
			pwm1_rising_moment = us;
		} else {
			pwm1_high_time = us - pwm1_rising_moment;
		}
		pwm1_previous_level = level;
	} else {
		if (level > 0 && pwm2_previous_level <= 0) {
			pwm2_period = us - pwm2_rising_moment;
			pwm2_rising_moment = us;
		} else {
			pwm2_high_time = us - pwm2_rising_moment;
		}
		pwm2_previous_level = level;
	}
}

void IRAM_ATTR
pwm_reader_task(void *arg)
{
	struct task_descriptor *task = arg;
	while (true) {
		if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
			pwm_reader_measurement_time = esp_timer_get_time();
			if (pwm_reader_measurement_time < us + FADE_OUT_PERIOD_US) {
				pwm1_frequency  = 1000000.0 / pwm1_period;
				pwm2_frequency  = 1000000.0 / pwm2_period;
				pwm1_duty_cycle = 100.0 * pwm1_high_time / pwm1_period;
				pwm2_duty_cycle = 100.0 * pwm2_high_time / pwm2_period;
			} else {
				pwm1_frequency  = 0;
				pwm2_frequency  = 0;
				pwm1_duty_cycle = pwm1_previous_level > 0 ? 100 : 0;
				pwm2_duty_cycle = pwm2_previous_level > 0 ? 100 : 0;
			}
			xSemaphoreGive(task->mutex);
		}
		TASK_DELAY_MS(task->performer_period_ms);
	}
	vTaskDelete(NULL);
}

int
pwm_reader_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%.0lf,%.0lf,%.0lf,%.0lf\n",
			task->prefix,
			pwm_reader_measurement_time / 1000,
			pwm1_frequency,
			pwm1_duty_cycle,
			pwm2_frequency,
			pwm2_duty_cycle
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
