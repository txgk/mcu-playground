#include "nosedive.h"

#define PWM_READER_MESSAGE_SIZE 200

void IRAM_ATTR
pwm_reader_task(void *dummy)
{
	char pwm_text_buf[PWM_READER_MESSAGE_SIZE];
	while (true) {
		int64_t ms = esp_timer_get_time() / 1000;
		int pwm_text_len = snprintf(
			pwm_text_buf,
			PWM_READER_MESSAGE_SIZE,
			"PWM@%lld=%.2lf,%.2lf,%.2lf,%.2lf\n",
			ms,
			get_pwm1_frequency(),
			get_pwm1_duty_cycle(),
			get_pwm2_frequency(),
			get_pwm2_duty_cycle()
		);
		if (pwm_text_len > 0 && pwm_text_len < PWM_READER_MESSAGE_SIZE) {
			send_data(pwm_text_buf, pwm_text_len);
			uart_write_bytes(UART_NUM_0, pwm_text_buf, pwm_text_len);
		}
		TASK_DELAY_MS(PWM_READER_POLLING_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
