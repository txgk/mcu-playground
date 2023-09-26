#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "usb_stream.h"
#include "candela.h"

// #define DEMO_UVC_FRAME_WIDTH     FRAME_RESOLUTION_ANY
// #define DEMO_UVC_FRAME_HEIGHT    FRAME_RESOLUTION_ANY
#define DEMO_UVC_FRAME_WIDTH        640
#define DEMO_UVC_FRAME_HEIGHT       480
#define DEMO_UVC_FRAME_INTERVAL     FPS2INTERVAL(15)
#define DEMO_UVC_XFER_BUFFER_SIZE   300000

volatile bool they_want_us_to_restart = false;

static volatile bool connected_to_wifi = false;
static volatile bool did_camera_reset = false;

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

static void
camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
	char msg[100];
	int msg_len = snprintf(msg, 100, "%" PRIu32 "x%" PRIu32 "\n", frame->width, frame->height);
	if (msg_len > 0 && msg_len < 100) write_websocket_message(msg, msg_len);
}

static void
stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
	switch (event) {
	case STREAM_CONNECTED: {
		write_websocket_message("connected\n", 10);
		// if (did_camera_reset == false) {
		// 	usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL);
		// 	uvc_frame_size_reset(DEMO_UVC_FRAME_WIDTH, DEMO_UVC_FRAME_HEIGHT, DEMO_UVC_FRAME_INTERVAL);
		// 	usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
		// 	did_camera_reset = true;
		// }
#if 0
		char msg[100];
		int msg_len;
		size_t frame_size = 0;
		size_t frame_index = 0;
		uvc_frame_size_list_get(NULL, &frame_size, &frame_index);
		if (frame_size) {
			msg_len = snprintf(msg, 100, "UVC: get frame list size = %u, current = %u\n", frame_size, frame_index);
			if (msg_len > 0 && msg_len < 100) write_websocket_message(msg, msg_len);
			uvc_frame_size_t *uvc_frame_list = (uvc_frame_size_t *)malloc(frame_size * sizeof(uvc_frame_size_t));
			uvc_frame_size_list_get(uvc_frame_list, NULL, NULL);
			for (size_t i = 0; i < frame_size; i++) {
				msg_len = snprintf(msg, 100, "frame[%u] = %ux%u\n", i, uvc_frame_list[i].width, uvc_frame_list[i].height);
				if (msg_len > 0 && msg_len < 100) write_websocket_message(msg, msg_len);
			}
			free(uvc_frame_list);
		} else {
			msg_len = snprintf(msg, 100, "UVC: get frame list size = %u\n", frame_size);
			if (msg_len > 0 && msg_len < 100) write_websocket_message(msg, msg_len);
		}
#endif
		break;
	}
	case STREAM_DISCONNECTED:
		write_websocket_message("disconnect\n", 11);
		break;
	default:
		write_websocket_message("unknown event\n", 14);
		break;
	}
}

void
app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	gpio_config_t led_indicator_cfg = {
		.pin_bit_mask = (1ULL << 15),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config(&led_indicator_cfg);
	gpio_set_level(15, 1);

	esp_log_set_vprintf(write_websocket_message_vprintf);

	esp_netif_init();
	esp_event_loop_create_default();
#ifdef CANDELA_USE_WIFI_STATION
	esp_netif_t *sta = esp_netif_create_default_wifi_sta();
	esp_netif_dhcpc_stop(sta);
	esp_netif_ip_info_t ip_info = {
		.ip      = {.addr = ESP_IP4TOADDR(192, 168,   0,  99)},
		.gw      = {.addr = ESP_IP4TOADDR(192, 168,   0,   1)},
		.netmask = {.addr = ESP_IP4TOADDR(255, 255, 255,   0)}
		// .ip      = {.addr = ESP_IP4TOADDR(192, 168,   102,  68)},
		// .gw      = {.addr = ESP_IP4TOADDR(192, 168,   102,  99)},
		// .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255,   0)}
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
	esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,  &wifi_birth, NULL, NULL);
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
	esp_wifi_set_max_tx_power(40); // 40 * 0.25 = 10 dB

	start_websocket_streamer();

	// while (true) {
	// 	TASK_DELAY_MS(500);
	// 	write_websocket_message("hi\n", 3);
	// }

	TASK_DELAY_MS(2000);
	write_websocket_message("hello world\n", 12);

	TASK_DELAY_MS(2000);
	write_websocket_message("allocing mem\n", 13);

	/* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
	uint8_t *xfer_buffer_a = malloc(DEMO_UVC_XFER_BUFFER_SIZE);
	uint8_t *xfer_buffer_b = malloc(DEMO_UVC_XFER_BUFFER_SIZE);
	uint8_t *frame_buffer = malloc(DEMO_UVC_XFER_BUFFER_SIZE);

	if (xfer_buffer_a == NULL || xfer_buffer_b == NULL || frame_buffer == NULL) {
		while (true) {
			write_websocket_message("Not enough memory!\n", 19);
			TASK_DELAY_MS(2000);
		}
	}

	uvc_config_t uvc_config = {
		/* match the any resolution of current camera (first frame size as default) */
		.frame_width = DEMO_UVC_FRAME_WIDTH,
		.frame_height = DEMO_UVC_FRAME_HEIGHT,
		.frame_interval = DEMO_UVC_FRAME_INTERVAL,
		.xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
		.xfer_buffer_a = xfer_buffer_a,
		.xfer_buffer_b = xfer_buffer_b,
		.frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
		.frame_buffer = frame_buffer,
		.frame_cb = &camera_frame_cb,
		.frame_cb_arg = NULL,
		.ep_addr = 0x82,
		.ep_mps = 1024,
		.interface = 1,
		.interface_alt = 1,
	};
	/* config to enable uvc function */
	if (uvc_streaming_config(&uvc_config) == ESP_OK) {
		/* register the state callback to get connect/disconnect event in the callback, we can get the frame list of current device */
		if (usb_streaming_state_register(&stream_state_changed_cb, NULL) == ESP_OK) {
			/* start usb streaming, UVC and UAC MIC will start streaming because SUSPEND_AFTER_START flags not set */
			if (usb_streaming_start() != ESP_OK) {
				write_websocket_message("usb_streaming_start fail\n", 25);
			}
		} else {
			write_websocket_message("usb_streaming_state_register fail\n", 34);
		}
	} else {
		write_websocket_message("uvc_streaming_config failed\n", 28);
	}

	while (true) {
		write_websocket_message("running\n", 8);
		TASK_DELAY_MS(5000);
	}

	esp_restart();
}
