#include "candela.h"

struct task_descriptor tasks[] = {
	{NULL, NULL, NULL, 0, 0, 0, 0, {0}, {{0}, {0}, {0}}, NULL},
};

void
start_tasks(void)
{
	for (size_t i = 0; tasks[i].prefix != NULL; ++i) {
		tasks[i].mutex = xSemaphoreCreateMutex();
		xTaskCreatePinnedToCore(tasks[i].performer, tasks[i].prefix, tasks[i].stack_size, &tasks[i], tasks[i].priority, NULL, 1);
	}
}
