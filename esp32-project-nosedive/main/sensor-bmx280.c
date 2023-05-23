#include <stdio.h>
#include "nosedive.h"
#include "bmx280.h"

static char bmx280_text_buf[200];
static int bmx280_text_len;

void
bmx280_task(void *dummy)
{
	bmx280_t *bmx280 = bmx280_create(I2C_NUM_0);
	if (bmx280 == NULL) {
		vTaskDelete(NULL);
	}
	bmx280_init(bmx280);
	bmx280_config_t bmx_cfg = BMX280_DEFAULT_CONFIG;
	bmx280_configure(bmx280, &bmx_cfg);
	while (true) {
		bmx280_setMode(bmx280, BMX280_MODE_FORCE);
		do {
			TASK_DELAY_MS(100);
		} while(bmx280_isSampling(bmx280));
		int64_t ms = esp_timer_get_time() / 1000;
		float temp = 0, pres = 0, hum = 0;
		bmx280_readoutFloat(bmx280, &temp, &pres, &hum);
		bmx280_text_len = snprintf(
			bmx280_text_buf,
			200,
			"BME280@%lld=%.2f,%.2f,%.2f\n",
			ms,
			temp,
			pres,
			hum
		);
		if (bmx280_text_len > 0 && bmx280_text_len < 200) {
			send_data(bmx280_text_buf, bmx280_text_len);
			uart_write_bytes(UART_NUM_0, bmx280_text_buf, bmx280_text_len);
		}
		TASK_DELAY_MS(1000);
	}
	vTaskDelete(NULL);
}
