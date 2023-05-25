#include <stdio.h>
#include "nosedive.h"
#include "lis3dh.h"

static char lis3dh_text_buf[200];
static int lis3dh_text_len;

void IRAM_ATTR
lis3dh_task(void *dummy)
{
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int id = lis3dh_read_device_id();
		lis3dh_text_len = snprintf(
			lis3dh_text_buf,
			200,
			"LIS3DH@%lld=%d\n",
			ms,
			id
		);
		if (lis3dh_text_len > 0 && lis3dh_text_len < 200) {
			send_data(lis3dh_text_buf, lis3dh_text_len);
			uart_write_bytes(UART_NUM_0, lis3dh_text_buf, lis3dh_text_len);
		}
		TASK_DELAY_MS(1000);
	}
	vTaskDelete(NULL);
}
