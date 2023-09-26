#include <assert.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "libuvc/libuvc.h"
#include "libuvc_helper.h"
#include "libuvc_adapter.h"
#include "usb/usb_host.h"
#include "candela.h"

#define FPS 15
#define WIDTH 640 // 256
#define HEIGHT 480 // 192
#define FORMAT UVC_FRAME_FORMAT_YUYV

static const char *TAG = "example";

static SemaphoreHandle_t ready_to_uninstall_usb;
static EventGroupHandle_t app_flags;

volatile bool they_want_us_to_restart = false;

static volatile bool connected_to_wifi = false;

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
usb_lib_handler_task(void *args)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
        // Give ready_to_uninstall_usb semaphore to indicate that USB Host library
        // can be deinitialized, and terminate this task.
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            xSemaphoreGive(ready_to_uninstall_usb);
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t
initialize_usb_host_lib(void)
{
    TaskHandle_t task_handle = NULL;

    const usb_host_config_t host_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        return err;
    }

    ready_to_uninstall_usb = xSemaphoreCreateBinary();
    if (ready_to_uninstall_usb == NULL) {
        usb_host_uninstall();
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(usb_lib_handler_task, "usb_events", 4096, NULL, 2, &task_handle) != pdPASS) {
        vSemaphoreDelete(ready_to_uninstall_usb);
        usb_host_uninstall();
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void
uninitialize_usb_host_lib(void)
{
    xSemaphoreTake(ready_to_uninstall_usb, portMAX_DELAY);
    vSemaphoreDelete(ready_to_uninstall_usb);

    if ( usb_host_uninstall() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to uninstall usb_host");
    }
}

static void
libuvc_adapter_cb(libuvc_adapter_event_t event)
{
    xEventGroupSetBits(app_flags, event);
}

void
frame_callback(uvc_frame_t *frame, void *ptr)
{
    static size_t fps;
    static size_t bytes_per_second;
    static int64_t start_time;

    int64_t current_time = esp_timer_get_time();
    bytes_per_second += frame->data_bytes;
    fps++;

    if (!start_time) {
        start_time = current_time;
    }

    if (current_time > start_time + 1000000) {
        ESP_LOGI(TAG, "fps: %u, bytes per second: %u", fps, bytes_per_second);
        start_time = current_time;
        bytes_per_second = 0;
        fps = 0;
    }

    // Stream received frame to client, if enabled
    // tcp_server_send(frame->data, frame->data_bytes);
    write_websocket_message("got frame\n", 10);
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
		// .ip      = {.addr = ESP_IP4TOADDR(192, 168, 102,  68)},
		// .gw      = {.addr = ESP_IP4TOADDR(192, 168, 102,  99)},
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

	start_websocket_streamer();

	// while (true) {
	// 	write_websocket_message("Started!\n", 9);
	// 	TASK_DELAY_MS(1000);
	// }

	uvc_context_t *ctx;
	uvc_device_t *dev;
	uvc_device_handle_t *devh;
	uvc_stream_ctrl_t ctrl;
	uvc_error_t res;

	app_flags = xEventGroupCreate();
	if (!app_flags) {
		while (true) {
			write_websocket_message("xEventGroupCreate failed\n", 25);
			TASK_DELAY_MS(5000);
		}
	}

	if (initialize_usb_host_lib() != ESP_OK) {
		while (true) {
			write_websocket_message("initialize_usb_host_lib failed\n", 31);
			TASK_DELAY_MS(5000);
		}
	}

	libuvc_adapter_config_t config = {
		.create_background_task = true,
		.task_priority = 5,
		.stack_size = 4096,
		.callback = libuvc_adapter_cb
	};
	libuvc_adapter_set_config(&config);

	if (uvc_init(&ctx, NULL) != UVC_SUCCESS) {
		while (true) {
			write_websocket_message("uvc_init failed\n", 16);
			TASK_DELAY_MS(5000);
		}
	}

	while (true) {
		if (uvc_find_device(ctx, &dev, 0, 0, NULL) != UVC_SUCCESS) {
			write_websocket_message("no device\n", 10);
			TASK_DELAY_MS(2000);
			continue;
		}
		write_websocket_message("connected\n", 10);
		if (uvc_open(dev, &devh) != UVC_SUCCESS) {
			write_websocket_message("uvc_open failed\n", 16);
			continue;
		}
		res = uvc_get_stream_ctrl_format_size(devh, &ctrl, FORMAT, WIDTH, HEIGHT, FPS);
		while (res != UVC_SUCCESS) {
			write_websocket_message("Negotiating streaming format failed, trying again...\n", 53);
			res = uvc_get_stream_ctrl_format_size(devh, &ctrl, FORMAT, WIDTH, HEIGHT, FPS);
			TASK_DELAY_MS(1000);
		}

		// dwMaxPayloadTransferSize has to be overwritten to MPS (maximum packet size)
		// supported by ESP32-S2(S3), as libuvc selects the highest possible MPS by default.
		ctrl.dwMaxPayloadTransferSize = 512;

		uvc_print_stream_ctrl(&ctrl, stderr);

		if (uvc_start_streaming(devh, &ctrl, frame_callback, NULL, 0) < 0) {
			write_websocket_message("uvc_start_streaming failed\n", 27);
			uvc_close(devh);
			continue;
		}

		uvc_stop_streaming(devh);
		uvc_close(devh);

		TASK_DELAY_MS(5000);
	}

	esp_restart();
}
