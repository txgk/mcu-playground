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
#include "driver/uart.h"
#include "nosedive.h"

static EventGroupHandle_t wifi_event_group; // For signaling when we are connected.
#define WIFI_CONNECTED_BIT BIT0

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
	uart_param_config(port, cfg);
	uart_set_pin(port, tx_pin, rx_pin, -1, -1);
}

static void
wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void
app_main(void)
{
	uart_config_t uart0_cfg = {0};
	uart_config_t uart1_cfg = {0};
	setup_serial(&uart0_cfg, UART_NUM_0, UART0_SPEED, UART0_TX_PIN, UART0_RX_PIN);
	setup_serial(&uart1_cfg, UART_NUM_1, UART1_SPEED, UART1_TX_PIN, UART1_RX_PIN);

	wifi_event_group = xEventGroupCreate();
	esp_netif_init();
	esp_event_loop_create_default();
	esp_netif_t *sta = esp_netif_create_default_wifi_sta();
	esp_netif_dhcpc_stop(sta);
	esp_netif_ip_info_t ip_info = {
		.ip = {.addr = ESP_IP4TOADDR(192, 168, 102, 43)},    // Адрес
		.gw = {.addr = ESP_IP4TOADDR(192, 168, 102, 99)},    // Шлюз
		.netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)} // Маска
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
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	esp_event_handler_instance_register(
		WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifi_event_handler,
		NULL,
		&instance_any_id
	);
	esp_event_handler_instance_register(
		IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&wifi_event_handler,
		NULL,
		&instance_got_ip
	);
	wifi_config_t wifi_cfg = {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASSWORD,
			.scan_method = WIFI_FAST_SCAN,
		}
	};
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
	esp_wifi_start();

	start_http_server();

	while (true) {
		TASK_DELAY_MS(1000);
	}
}
