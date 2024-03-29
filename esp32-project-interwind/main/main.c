#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "interwind.h"
#include "driver-pwm-reader.h"

volatile bool they_want_us_to_restart = false;

static volatile bool connected_to_wifi = false;

adc_oneshot_unit_handle_t adc1_handle;

static inline void
setup_serial(uart_config_t *cfg, uart_port_t port, int speed, int tx_pin, int rx_pin)
{
	cfg->baud_rate = speed;
	cfg->data_bits = UART_DATA_8_BITS;
	cfg->parity    = UART_PARITY_DISABLE;
	cfg->stop_bits = UART_STOP_BITS_1;
	cfg->flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
	cfg->rx_flow_ctrl_thresh = 122;
	// cfg->source_clk = UART_SCLK_DEFAULT;
#if CONFIG_UART_ISR_IN_IRAM
	uart_driver_install(port, 1024, 1024, 0, NULL, ESP_INTR_FLAG_IRAM);
#else
	uart_driver_install(port, 1024, 1024, 0, NULL, 0);
#endif
	uart_param_config(port, cfg);
	uart_set_pin(port, tx_pin, rx_pin, -1, -1);
}

static void
wifi_birth(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		connected_to_wifi = true;
	}
}

static void
wifi_death(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
		connected_to_wifi = false;
	}
}

static void IRAM_ATTR
wifi_loop(void *dummy)
{
	while (true) {
		if (connected_to_wifi == false) {
			esp_wifi_connect();
		}
		TASK_DELAY_MS(WIFI_RECONNECTION_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void
tell_esp_to_restart(const char *value, char *answer_buf_ptr, int *answer_len_ptr)
{
	they_want_us_to_restart = true;
}

void
app_main(void)
{
	nvs_flash_init();
	gpio_install_isr_service(0);

	gpio_config_t pwm_input_cfg = {
		.pin_bit_mask = (1ULL << PWM1_INPUT_PIN) | (1ULL << PWM2_INPUT_PIN) | (1ULL << PWM3_INPUT_PIN),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_ANYEDGE,
	};
	gpio_config(&pwm_input_cfg);
	gpio_isr_handler_add(PWM1_INPUT_PIN, &trigger_pwm_calculation, (void *)0);
	gpio_isr_handler_add(PWM2_INPUT_PIN, &trigger_pwm_calculation, (void *)1);
	gpio_isr_handler_add(PWM3_INPUT_PIN, &trigger_pwm_calculation, (void *)2);

	uart_config_t uart0_cfg = {0};
	setup_serial(&uart0_cfg, UART_NUM_0, UART0_SPEED, UART0_TX_PIN, UART0_RX_PIN);

	i2c_config_t i2c_cfg = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C1_SDA_PIN,
		.scl_io_num = I2C1_SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C1_SPEED
	};
	i2c_param_config(I2C_NUM_0, &i2c_cfg);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

	adc_oneshot_unit_init_cfg_t adc1_init_cfg = {.unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE};
	adc_oneshot_new_unit(&adc1_init_cfg, &adc1_handle);
	adc_oneshot_chan_cfg_t therm_adc_cfg = {.bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_11};
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_0_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_1_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_2_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_3_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_4_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_5_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_6_ADC_CH, &therm_adc_cfg);
	adc_oneshot_config_channel(adc1_handle, THERMISTOR_7_ADC_CH, &therm_adc_cfg);

#if 0 // Для отладки датчика PWM.
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (17) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (820) // Set duty to 10%. ((2 ** 13) - 1) * 10% = 820
#define LEDC_FREQUENCY          (1200) // Frequency in Hertz. Set frequency at 1.2 kHz
	ledc_timer_config_t ledc_timer = {
		.speed_mode       = LEDC_MODE,
		.timer_num        = LEDC_TIMER,
		.duty_resolution  = LEDC_DUTY_RES,
		.freq_hz          = LEDC_FREQUENCY,
		.clk_cfg          = LEDC_AUTO_CLK
	};
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
	ledc_channel_config_t ledc_channel = {
		.speed_mode     = LEDC_MODE,
		.channel        = LEDC_CHANNEL,
		.timer_sel      = LEDC_TIMER,
		.intr_type      = LEDC_INTR_DISABLE,
		.gpio_num       = LEDC_OUTPUT_IO,
		.duty           = 0, // Set duty to 0%
		.hpoint         = 0
	};
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
	ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
#endif

	esp_netif_init();
	esp_event_loop_create_default();
#ifdef INTERWIND_USE_WIFI_STATION
	esp_netif_t *sta = esp_netif_create_default_wifi_sta();
	esp_netif_dhcpc_stop(sta);
	esp_netif_ip_info_t ip_info = {
		.ip = {.addr = ESP_IP4TOADDR(192, 168,   0,  99)},
		.gw = {.addr = ESP_IP4TOADDR(192, 168,   0,   1)},
		.netmask = {.addr = ESP_IP4TOADDR(255, 255, 255,   0)}
	};
	esp_netif_set_ip_info(sta, &ip_info);
	esp_netif_dns_info_t dns1 = {.ip = INIT_IP4_LOL(77, 88, 8, 8)};
	esp_netif_dns_info_t dns2 = {.ip = INIT_IP4_LOL(77, 88, 8, 1)};
	esp_netif_dns_info_t dns3 = {.ip = INIT_IP4_LOL(77, 88, 8, 88)};
	esp_netif_set_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns1);
	esp_netif_set_dns_info(sta, ESP_NETIF_DNS_BACKUP, &dns2);
	esp_netif_set_dns_info(sta, ESP_NETIF_DNS_FALLBACK, &dns3);
	wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&wifi_init_cfg);
	esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_birth, NULL, NULL);
	esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &wifi_death, NULL, NULL);
	wifi_config_t wifi_cfg = {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASSWORD,
		}
	};
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
	esp_wifi_start();
	esp_wifi_set_ps(WIFI_PS_NONE);
	xTaskCreatePinnedToCore(&wifi_loop, "wifi_loop", 2048, NULL, 1, NULL, 1);
#else
	esp_netif_create_default_wifi_ap();
	wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&wifi_init_cfg);
	wifi_config_t wifi_cfg = {
		.ap = {
			.ssid = WIFI_AP_SSID,
			.ssid_len = sizeof(WIFI_AP_SSID) - 1,
			.password = WIFI_AP_PASS,
			.channel = 7,
			.max_connection = 4,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg = {
				.required = true,
			},
		}
	};
	esp_wifi_set_mode(WIFI_MODE_AP);
	esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
	esp_wifi_start();
	esp_wifi_set_ps(WIFI_PS_NONE);
#endif

	// Start tasks before starting streamers because they don't have mutexes yet!
	start_tasks();

	start_websocket_streamer();
	start_serial_streamer();
	start_http_tuner();

	// Здесь мы формируем heartbeat пакеты, пока не придёт команда перезагрузки.
	// char heartbeat_text_buf[100];
	// int32_t i = 1;
	while (they_want_us_to_restart == false) {
		// int64_t ms = esp_timer_get_time() / 1000;
		// int heartbeat_text_len = snprintf(heartbeat_text_buf, 100, "BEAT@%lld=%ld\n", ms, i);
		// if (heartbeat_text_len > 0 && heartbeat_text_len < 100) {
		// 	send_data(heartbeat_text_buf, heartbeat_text_len);
			// uart_write_bytes(UART_NUM_0, "gaga", 4);
		// }
		// i = (i * 10) % 999999999;
		TASK_DELAY_MS(1000);
	}

	esp_restart();
}
