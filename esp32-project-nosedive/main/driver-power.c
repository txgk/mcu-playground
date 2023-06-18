#include "nosedive.h"

static volatile int64_t power_measurement_time = 0;
static volatile int power_power_on             = 0;
static volatile int power_kill_switch          = 0;
static volatile int power_current_alert        = 0;

void IRAM_ATTR
power_task(void *arg)
{
	struct task_descriptor *task = arg;
	while (true) {
		if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
			power_measurement_time = esp_timer_get_time();
			power_power_on         = gpio_get_level(POWER_ON_PIN);
			power_kill_switch      = gpio_get_level(KILL_SWITCH_PIN);
			power_current_alert    = gpio_get_level(CURRENT_ALERT_PIN);
			xSemaphoreGive(task->mutex);
		}
		TASK_DELAY_MS(task->performer_period_ms);
	}
	vTaskDelete(NULL);
}

int
power_info(struct task_descriptor *task, char *dest)
{
	int len = 0;
	if (xSemaphoreTake(task->mutex, portMAX_DELAY) == pdTRUE) {
		len = snprintf(dest, MESSAGE_SIZE_LIMIT,
			"%s@%lld=%d,%d,%d\n",
			task->prefix,
			power_measurement_time / 1000,
			power_kill_switch,
			power_power_on,
			power_current_alert
		);
		xSemaphoreGive(task->mutex);
	}
	return len > 0 && len < MESSAGE_SIZE_LIMIT ? len : 0;
}
