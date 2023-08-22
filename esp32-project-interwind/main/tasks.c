#include "interwind.h"
#include "driver-pwm-reader.h"
#include "driver-tpa626.h"
#include "driver-thermistors.h"

struct task_descriptor tasks[] = {
	{"PWM1",    &pwm_reader_task, &pwm_reader_info, 1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"PWM2",    &pwm_reader_task, &pwm_reader_info, 1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"PWM3",    &pwm_reader_task, &pwm_reader_info, 1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"TPA626",  &tpa626_task,     &tpa626_info,     1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"THERM",   &therm_task,      &therm_info,      1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{NULL,      NULL,             NULL,                0,    0,    0, 0, {0}, {{0}, {0}, {0}}, NULL},
};

void
start_tasks(void)
{
	for (size_t i = 0; tasks[i].prefix != NULL; ++i) {
		tasks[i].mutex = xSemaphoreCreateMutex();
		xTaskCreatePinnedToCore(tasks[i].performer, tasks[i].prefix, tasks[i].stack_size, &tasks[i], tasks[i].priority, NULL, 1);
	}
}
