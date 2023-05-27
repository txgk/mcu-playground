#include <stdio.h>
#include "nosedive.h"

#define NTC_SAMPLES_COUNT 64

static char ntc_text_buf[100];
static int ntc_text_len;

static adc_channel_t ntc_adc_channel_id = ADC_CHANNEL_5;
static adc_oneshot_chan_cfg_t ntc_adc_cfg = {
	.bitwidth = ADC_BITWIDTH_13,
	.atten = ADC_ATTEN_DB_11,
};
static adc_cali_handle_t ntc_adc_cali_handle = NULL;

void
init_adc_for_ntc_task(void)
{
	adc_unit_t adc_unit_id = ADC_UNIT_1;
	adc_oneshot_io_to_channel(NTC_PIN, &adc_unit_id, &ntc_adc_channel_id);
	adc_oneshot_config_channel(adc1_handle, ntc_adc_channel_id, &ntc_adc_cfg);
	ntc_adc_cali_handle = get_adc_channel_calibration_profile(adc_unit_id, ntc_adc_channel_id, ntc_adc_cfg.bitwidth, ntc_adc_cfg.atten);
}

void IRAM_ATTR
ntc_task(void *dummy)
{
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		uint64_t read_sum = 0;
		int millivolts = 0;
		for (uint8_t i = 0; i < NTC_SAMPLES_COUNT; ++i) {
			int read_sample = 0;
			adc_oneshot_read(adc1_handle, ntc_adc_channel_id, &read_sample);
			read_sum += read_sample;
		}
		if (ntc_adc_cali_handle != NULL) {
			adc_cali_raw_to_voltage(ntc_adc_cali_handle, read_sum / NTC_SAMPLES_COUNT, &millivolts);
		} else {
			millivolts = read_sum * 2450 / NTC_SAMPLES_COUNT / 4095;
		}
		ntc_text_len = snprintf(
			ntc_text_buf,
			100,
			"NTC@%lld=%d\n",
			ms,
			millivolts
		);
		if (ntc_text_len > 0 && ntc_text_len < 100) {
			send_data(ntc_text_buf, ntc_text_len);
			uart_write_bytes(UART_NUM_0, ntc_text_buf, ntc_text_len);
		}
		TASK_DELAY_MS(NTC_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
