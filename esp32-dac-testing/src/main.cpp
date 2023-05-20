#include <string.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "driver/dac.h"
#include "../../wifi-credentials.h"

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SDA2_PIN                         34
#define SCL2_PIN                         35
#define RX_PIN                           16
#define TX_PIN                           17
#define PWM_IN_1_PIN                     18
#define PWM_IN_2_PIN                     19
#define WIFI_DATA_PORT                   80

#define HEART_BEAT_PERIODICITY           2000 // milliseconds

#define TASK_DELAY_MS(A) vTaskDelay(A / portTICK_PERIOD_MS)
#define ISDIGIT(A) (((A)=='0')||((A)=='1')||((A)=='2')||((A)=='3')||((A)=='4')||((A)=='5')||((A)=='6')||((A)=='7')||((A)=='8')||((A)=='9'))

IPAddress            ip(192, 168, 102,  42);
IPAddress       gateway(192, 168, 102,  99);
IPAddress        subnet(255, 255, 255,   0);
IPAddress   primary_dns( 77,  88,   8,   8);
IPAddress secondary_dns( 77,  88,   8,   1);

SemaphoreHandle_t wifi_lock = NULL;
WiFiServer streamer(WIFI_DATA_PORT);
WiFiClient listener;

volatile bool beat_allowed_to_run = true, beat_has_stopped = false;

volatile bool dac1_enabled = true;
volatile bool dac2_enabled = true;

void
send_data(const char *data, size_t data_len)
{
	if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
		if (listener) {
			listener.write(data, data_len);
		} else {
			listener = streamer.available();
			if (listener) {
				listener.write(data, data_len);
			}
		}
		xSemaphoreGive(wifi_lock);
	}
}

void IRAM_ATTR
beat_loop(void *dummy)
{
	char beat_buf[100];
	unsigned i = 1;
	while (beat_allowed_to_run == true) {
		int beat_len = sprintf(beat_buf, "\nBEAT@%lu=%u", millis(), i);
		if (beat_len > 0 && beat_len < 100) send_data(beat_buf, beat_len);
		i = (i * 10) % 9999;
		TASK_DELAY_MS(HEART_BEAT_PERIODICITY);
	}
	beat_has_stopped = true;
	vTaskDelete(NULL);
}

void IRAM_ATTR
pwm_monitor_loop(void *dummy)
{
#define PWMS_BUFFER_SIZE 500
	char pwms[PWMS_BUFFER_SIZE];
	size_t pwms_len;
	dac_output_enable(DAC_CHANNEL_1);
	dac_output_enable(DAC_CHANNEL_2);
	while (true) {
		// pulseIn returns microseconds!
		unsigned long packet_birth = millis();
		unsigned long pwm1_high    = pulseIn(PWM_IN_1_PIN, HIGH, 50000);
		unsigned long pwm1_low     = pulseIn(PWM_IN_1_PIN, LOW,  50000);
		unsigned long pwm2_high    = pulseIn(PWM_IN_2_PIN, HIGH, 50000);
		unsigned long pwm2_low     = pulseIn(PWM_IN_2_PIN, LOW,  50000);
		unsigned long pwm1_min     = pwm1_high < pwm1_low || pwm1_low == 0 ? pwm1_high : pwm1_low;
		unsigned long pwm2_min     = pwm2_high < pwm2_low || pwm2_low == 0 ? pwm2_high : pwm2_low;
		unsigned long pwm1_period  = pwm1_high + pwm1_low;
		unsigned long pwm2_period  = pwm2_high + pwm2_low;
		double        pwm1_freq    = 1000000.0 / pwm1_period;
		double        pwm2_freq    = 1000000.0 / pwm2_period;
		if (pwm1_min <= 1050) {
			dac_output_voltage(DAC_CHANNEL_1, 0);
		} else if (pwm1_min >= 2000) {
			dac_output_voltage(DAC_CHANNEL_1, 255);
		} else {
			dac_output_voltage(DAC_CHANNEL_1, (pwm1_min - 1000) * 255 / 1000);
		}
		if (pwm2_min <= 1050) {
			dac_output_voltage(DAC_CHANNEL_2, 0);
		} else if (pwm2_min >= 2000) {
			dac_output_voltage(DAC_CHANNEL_2, 255);
		} else {
			dac_output_voltage(DAC_CHANNEL_2, (pwm2_min - 1000) * 255 / 1000);
		}
		// pwms_len = snprintf(pwms, PWMS_BUFFER_SIZE,
		// 	"\nPWMRC1@%lu=%.1lf,%lu,%lu",
		// 	packet_birth,
		// 	pwm1_freq,
		// 	pwm1_min,
		// 	pwm1_period
		// );
		// if (pwms_len < PWMS_BUFFER_SIZE) send_data(pwms, pwms_len);
		// pwms_len = snprintf(pwms, PWMS_BUFFER_SIZE,
		// 	"\nPWMRC2@%lu=%.1lf,%lu,%lu",
		// 	packet_birth,
		// 	pwm2_freq,
		// 	pwm2_min,
		// 	pwm2_period
		// );
		// if (pwms_len < PWMS_BUFFER_SIZE) send_data(pwms, pwms_len);
		TASK_DELAY_MS(50);
	}
	vTaskDelete(NULL);
}

void
start_beat_loop(void)
{
	xTaskCreatePinnedToCore(
		&beat_loop,
		"beat_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
start_pwm_monitor_loop(void)
{
	xTaskCreatePinnedToCore(
		&pwm_monitor_loop,
		"pwm_monitor_loop",
		4096, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
setup(void)
{
	gpio_config_t input_cfg = {
		(1ULL << PWM_IN_1_PIN) | (1ULL << PWM_IN_2_PIN),
		GPIO_MODE_INPUT,
		GPIO_PULLUP_DISABLE,
		GPIO_PULLDOWN_DISABLE,
		GPIO_INTR_DISABLE,
	};
	gpio_config(&input_cfg);
	wifi_lock = xSemaphoreCreateMutex();
	// WiFi.config(ip, gateway, subnet, primary_dns, secondary_dns);
	// WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	// while (WiFi.status() != WL_CONNECTED) {
	// 	delay(500);
	// }
	// ArduinoOTA.begin();
	// streamer.begin();
	start_beat_loop();
	start_pwm_monitor_loop();
}

void
loop(void)
{
	// if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
	// 	ArduinoOTA.handle();
	// 	xSemaphoreGive(wifi_lock);
	// }
	delay(1000);
}
