#include "nosedive.h"
#include "driver-bmx280.h"
#include "driver-hall.h"
#include "driver-lis3dh.h"
#include "driver-max6675.h"
#include "driver-pwm-reader.h"
#include "driver-tpa626.h"
#include "driver-ntc.h"
#include "driver-speed.h"
#include "driver-power.h"

struct task_descriptor tasks[] = {
	{"BME280",  &bmx280_task,     &bmx280_info,     1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"LIS3DH",  &lis3dh_task,     &lis3dh_info,     1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"MAX6675", &max6675_task,    &max6675_info,    1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"HALL",    &hall_task,       &hall_info,       1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"PWM",     &pwm_reader_task, &pwm_reader_info, 1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"TPA626",  &tpa626_task,     &tpa626_info,     1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"NTC",     &ntc_task,        &ntc_info,        1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"SPEED",   &speed_task,      &speed_info,      1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{"POWER",   &power_task,      &power_info,      1000, 1000, 2048, 1, {0}, {{0}, {0}, {0}}, NULL},
	{NULL,      NULL,             NULL,             0,    0,    0,    0, {0}, {{0}, {0}, {0}}, NULL},
};

void
start_tasks(void)
{
	for (size_t i = 0; tasks[i].prefix != NULL; ++i) {
		tasks[i].mutex = xSemaphoreCreateMutex();
		xTaskCreatePinnedToCore(tasks[i].performer, tasks[i].prefix, tasks[i].stack_size, &tasks[i], tasks[i].priority, NULL, 1);
	}
}
