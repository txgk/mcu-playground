#include "nosedive.h"
#include "driver-hall.h"

volatile bool hall_intr_triggered = false;

// #define TIMER_RESOLUTION 1000000 // 1 MHz
// #define ALARM_PERIOD 1000000 // 1 s

// static gptimer_handle_t gptimer = NULL;

// static bool IRAM_ATTR
// register_hall_switch(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
// {
// 	BaseType_t high_task_awoken = pdFALSE;
// 	return high_task_awoken == pdTRUE;
// }

// static void IRAM_ATTR
// hall_handler(void *dummy)
// {
// 	hall_intr_triggered = true;
// }

bool
hall_initialize(void)
{
#ifdef READ_HALL_AS_ANALOG_PIN
	adc_oneshot_unit_init_cfg_t adc2_init_cfg = {.unit_id = ADC_UNIT_2};
	adc_oneshot_new_unit(&adc2_init_cfg, &adc2_handle);
	adc_oneshot_chan_cfg_t hall_adc_cfg  = {.bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_11};
	adc_oneshot_config_channel(adc2_handle, HALL_ADC_CHANNEL, &hall_adc_cfg);
#else
	gpio_config_t hall_cfg = {
		(1ULL << HALL_PIN),
		GPIO_MODE_INPUT,
		GPIO_PULLUP_DISABLE,
		GPIO_PULLDOWN_DISABLE,
		GPIO_INTR_DISABLE,
	};
	gpio_config(&hall_cfg);
#endif
	// gpio_config_t hall_cfg = {
	// 	(1ULL << HALL_PIN),
	// 	GPIO_MODE_INPUT,
	// 	GPIO_PULLUP_DISABLE,
	// 	GPIO_PULLDOWN_DISABLE,
	// 	GPIO_INTR_ANYEDGE,
	// };
	// gpio_install_isr_service(0);
	// gpio_isr_handler_add(HALL_PIN, &hall_handler, NULL);

	// esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_IRAM, &hall_handler, NULL, NULL);

	// gptimer_config_t gptimer_cfg = {
	// 	.clk_src = GPTIMER_CLK_SRC_DEFAULT,
	// 	.direction = GPTIMER_COUNT_UP,
	// 	.resolution_hz = TIMER_RESOLUTION,
	// };
	// gptimer_new_timer(&gptimer_cfg, &gptimer);
	// gptimer_event_callbacks_t callbacks_cfg = {
	// 	.on_alarm = &register_hall_switch,
	// };
	// gptimer_register_event_callbacks(gptimer, &callbacks_cfg, NULL);
	// gptimer_enable(gptimer);
	// gptimer_alarm_config_t alarm_cfg = {
	// 	.alarm_count = ALARM_PERIOD,
	// };
	// gptimer_set_alarm_action(gptimer, &alarm_cfg);
	// gptimer_enable(gptimer);
	return true;
}
