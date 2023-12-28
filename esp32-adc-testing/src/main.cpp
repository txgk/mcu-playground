#include <Arduino.h>
#include <WiFi.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "WebSocketsServer.h"
#include "../../wifi-credentials.h"

#define TASK_DELAY_MS(A) vTaskDelay(A / portTICK_PERIOD_MS)

#define WS_STREAMER_PORT  222

IPAddress            ip(192, 168, 102, 41);
IPAddress       gateway(192, 168, 102, 99);
IPAddress        subnet(255, 255, 255,  0);
IPAddress   primary_dns( 77,  88,   8,  8);
IPAddress secondary_dns( 77,  88,   8,  1);

WebSocketsServer ws = WebSocketsServer(WS_STREAMER_PORT);

#define LOG_SERIAL_SPEED                 9600
#define HEART_BEAT_PERIODICITY           2000 // milliseconds

SemaphoreHandle_t wifi_lock = NULL;

void
send_data(const char *data, size_t data_len)
{
	if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
		if (ws.connectedClients() > 0) ws.broadcastTXT(data, data_len);
		xSemaphoreGive(wifi_lock);
	}
}

void IRAM_ATTR
beat_loop(void *dummy)
{
	char beat_buf[100];
	for (unsigned i = 1; true; i = (i * 10) % 9999) {
		int beat_len = sprintf(beat_buf, "BEAT@%lu=%u\n", millis(), i);
		if (beat_len > 0 && beat_len < 100) send_data(beat_buf, beat_len);
		TASK_DELAY_MS(HEART_BEAT_PERIODICITY);
	}
	vTaskDelete(NULL);
}

void IRAM_ATTR
adc_loop(void *dummy)
{
	char out[100];
	gpio_config_t gpio_cfg = {
		(1ULL << 35),
		GPIO_MODE_INPUT,
		GPIO_PULLUP_DISABLE,
		GPIO_PULLDOWN_DISABLE,
		GPIO_INTR_DISABLE,
	};
	gpio_config(&gpio_cfg);
#define SAMPLES_COUNT 64
// #define CHAN ADC1_CHANNEL_4 // GPIO32
// #define CHAN ADC1_CHANNEL_5 // GPIO33
// #define CHAN ADC1_CHANNEL_6 // GPIO34
#define CHAN ADC1_CHANNEL_7 // GPIO35
// #define CHAN ADC1_CHANNEL_0 // GPIO36
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(CHAN, ADC_ATTEN_DB_11);
	while (true) {
		unsigned long packet_birth = millis();
		long sum = 0;
		for (size_t i = 0; i < SAMPLES_COUNT; ++i) {
			sum += adc1_get_raw(CHAN);
		}
		int len = snprintf(out, 100,
			"ADC_TEST@%lu=%d\n",
			packet_birth,
			sum / SAMPLES_COUNT
		);
		if (len > 0 && len < 100) send_data(out, len);
		TASK_DELAY_MS(200);
	}
	vTaskDelete(NULL);
}

void
ws_event_handler(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
	if (type == WStype_CONNECTED) {
		ws.broadcastTXT("BEAT@names=HeartBeat(hit)\n", 26);
		ws.broadcastTXT("ADC_TEST@names=AdcReading(int)\n", 31);
	}
}

void
setup(void)
{
	// Serial.begin(LOG_SERIAL_SPEED);
	wifi_lock = xSemaphoreCreateMutex();
	// WiFi.softAP("HahaHoho", "12345678");
	WiFi.config(ip, gateway, subnet, primary_dns, secondary_dns);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	// while (WiFi.status() != WL_CONNECTED) {
	// 	delay(500);
	// }
	ws.begin();
	ws.onEvent(ws_event_handler);
	xTaskCreatePinnedToCore(&beat_loop, "beat_loop", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(&adc_loop, "adc_loop", 4096, NULL, 1, NULL, 1);
	// Serial.println(WiFi.localIP());
}

void
loop(void)
{
	ws.loop();
}
