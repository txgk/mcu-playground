#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "candela.h"

volatile bool they_want_us_to_restart = false;

static volatile bool connected_to_wifi = false;

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

	uart_config_t uart0_cfg = {0};
	setup_serial(&uart0_cfg, UART_NUM_0, UART0_SPEED, UART0_TX_PIN, UART0_RX_PIN);

	twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
	twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
	if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
		if (twai_start() == ESP_OK) {
			tell_them_that_can_driver_is_started();
		}
	}

	esp_netif_init();
	esp_event_loop_create_default();
#ifdef CANDELA_USE_WIFI_STATION
	esp_netif_t *sta = esp_netif_create_default_wifi_sta();
	esp_netif_dhcpc_stop(sta);
	esp_netif_ip_info_t ip_info = {
		.ip      = {.addr = ESP_IP4TOADDR(192, 168,   0,  99)},
		.gw      = {.addr = ESP_IP4TOADDR(192, 168,   0,   1)},
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

	start_serial_streamer();
	start_websocket_streamer();
	start_http_tuner();
	start_can_listener();

	// Здесь мы формируем heartbeat пакеты, пока не придёт команда перезагрузки.
	// char heartbeat_text_buf[100];
	// int32_t i = 1;
	while (they_want_us_to_restart == false) {
		// int64_t ms = esp_timer_get_time() / 1000;
		// int heartbeat_text_len = snprintf(heartbeat_text_buf, 100, "BEAT@%lld=%ld\n", ms, i);
		// if (heartbeat_text_len > 0 && heartbeat_text_len < 100) {
		// 	send_data(heartbeat_text_buf, heartbeat_text_len);
		// 	uart_write_bytes(UART_NUM_0, heartbeat_text_buf, heartbeat_text_len);
		// }
		// i = (i * 10) % 999999999;
		TASK_DELAY_MS(1000);
	}

	esp_restart();
}
