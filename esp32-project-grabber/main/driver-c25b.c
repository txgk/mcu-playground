#include "main.h"

bool
c25b_driver_setup(void)
{
	gpio_config_t c25b_pins_cfg = {
		.pin_bit_mask = (1ULL << C25B_DE_PIN),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config(&c25b_pins_cfg);
	gpio_set_level(C25B_DE_PIN, 1);
	return true;
}
