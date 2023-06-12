#include "nosedive.h"

#define FADE_OUT_PERIOD_US 2000000

static volatile int pwm1_previous_level = 0;
static volatile int64_t pwm1_period = 0;
static volatile int64_t pwm1_high_time = 0;
static volatile int64_t pwm1_rising_moment = 0;
static volatile int pwm2_previous_level = 0;
static volatile int64_t pwm2_period = 0;
static volatile int64_t pwm2_high_time = 0;
static volatile int64_t pwm2_rising_moment = 0;
static volatile int64_t us = 0;

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

double
get_pwm1_frequency(void)
{
	return esp_timer_get_time() < us + FADE_OUT_PERIOD_US ? 1000000.0 / pwm1_period : 0;
}

double
get_pwm2_frequency(void)
{
	return esp_timer_get_time() < us + FADE_OUT_PERIOD_US ? 1000000.0 / pwm2_period : 0;
}

double
get_pwm1_duty_cycle(void)
{
	return esp_timer_get_time() < us + FADE_OUT_PERIOD_US ? 100.0 * pwm1_high_time / pwm1_period : 0;
}

double
get_pwm2_duty_cycle(void)
{
	return esp_timer_get_time() < us + FADE_OUT_PERIOD_US ? 100.0 * pwm2_high_time / pwm2_period : 0;
}
