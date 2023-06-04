#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nosedive.h"
#include "driver-pca9685.h"

SemaphoreHandle_t system_mutexes[NUMBER_OF_MUTEXES];
volatile bool they_want_us_to_restart = false;

static EventGroupHandle_t wifi_event_group; // For signaling when we are connected.
#define WIFI_CONNECTED_BIT BIT0

adc_oneshot_unit_handle_t adc1_handle;

static inline void
setup_serial(uart_config_t *cfg, uart_port_t port, int speed, int tx_pin, int rx_pin)
{
	int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
	intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
	cfg->baud_rate = speed;
	cfg->data_bits = UART_DATA_8_BITS;
	cfg->parity    = UART_PARITY_DISABLE;
	cfg->stop_bits = UART_STOP_BITS_1;
	cfg->flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
	cfg->rx_flow_ctrl_thresh = 122;
	// cfg->source_clk = UART_SCLK_DEFAULT;
	uart_driver_install(port, 1024, 1024, 0, NULL, intr_alloc_flags);
	uart_param_config(port, cfg);
	uart_set_pin(port, tx_pin, rx_pin, -1, -1);
}

static void
wifi_birth(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

static void
wifi_death(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
		xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

static void IRAM_ATTR
wifi_loop(void *dummy)
{
	EventBits_t old_bit = xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT;
	while (true) {
		TASK_DELAY_MS(WIFI_RECONNECTION_PERIOD_MS);
		EventBits_t new_bit = xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT;
		if (new_bit != old_bit) {
			old_bit = new_bit;
			if (new_bit & WIFI_CONNECTED_BIT) {
				start_http_streamer();
				start_http_tuner();
			} else {
				stop_http_streamer();
				stop_http_tuner();
			}
		}
		if ((new_bit & WIFI_CONNECTED_BIT) == 0) {
			esp_wifi_connect();
		}
	}
	vTaskDelete(NULL);
}

void
tell_esp_to_restart(const char *dummy)
{
	they_want_us_to_restart = true;
}

void
app_main(void)
{
	for (int i = 0; i < NUMBER_OF_MUTEXES; ++i) {
		system_mutexes[i] = xSemaphoreCreateMutex();
	}

	wifi_event_group = xEventGroupCreate();

	nvs_flash_init();

	uart_config_t uart0_cfg = {0};
	uart_config_t uart1_cfg = {0};
	setup_serial(&uart0_cfg, UART_NUM_0, UART0_SPEED, UART0_TX_PIN, UART0_RX_PIN);
	setup_serial(&uart1_cfg, UART_NUM_1, UART1_SPEED, UART1_TX_PIN, UART1_RX_PIN);

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

	spi_bus_config_t spi_cfg = {
		.mosi_io_num = SPI1_MOSI_PIN,
		.miso_io_num = SPI1_MISO_PIN,
		.sclk_io_num = SPI1_SCLK_PIN,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 32,
	};
	spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

	pca9685_initialize();
	pca9685_change_frequency(1);
	pca9685_channel_full_toggle(PCA9685_ALL, false);

	adc_oneshot_unit_init_cfg_t adc1_init_cfg = {
		.unit_id = ADC_UNIT_1,
	};
	adc_oneshot_new_unit(&adc1_init_cfg, &adc1_handle);
	init_adc_for_ntc_task();

	esp_netif_init();
	esp_event_loop_create_default();
#if 0 // WIFI STATION
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
#else // WIFI ACCESS POINT
	esp_netif_create_default_wifi_ap();
	wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&wifi_init_cfg);
	wifi_config_t wifi_cfg = {
		.ap = {
			.ssid = "demoproshivka",
			.ssid_len = 13,
			.password = "elbereth",
			.channel = 7,
			.max_connection = 3,
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
	start_http_streamer();
	start_http_tuner();
#endif

	xTaskCreatePinnedToCore(&bmx280_task, "bmx280_task", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(&tpa626_task, "tpa626_task", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(&lis3dh_task, "lis3dh_task", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(&ntc_task, "ntc_task", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(&max6675_task, "max6675_task", 2048, NULL, 1, NULL, 1);

	// Здесь мы формируем heartbeat пакеты, пока не придёт команда перезагрузки.
	char heartbeat_text_buf[100];
	int32_t i = 1;
	while (they_want_us_to_restart == false) {
		int64_t ms = esp_timer_get_time() / 1000;
		int heartbeat_text_len = snprintf(heartbeat_text_buf, 100, "BEAT@%lld=%ld\n", ms, i);
		if (heartbeat_text_len > 0 && heartbeat_text_len < 100) {
			send_data(heartbeat_text_buf, heartbeat_text_len);
			uart_write_bytes(UART_NUM_0, heartbeat_text_buf, heartbeat_text_len);
		}
		i = (i * 10) % 999999999;
		TASK_DELAY_MS(HEARTBEAT_PERIOD_MS);
	}

	esp_restart();
}
