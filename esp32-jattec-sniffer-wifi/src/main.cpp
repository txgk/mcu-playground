#include <Arduino.h>
#include <WiFi.h>
#include "wifi-credentials.h"

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SERIAL_SPEED                     115200
#define WIFI_DATA_PORT                   80
#define SDA                              ((REG_READ(GPIO_IN1_REG)) & 0b01)
#define SCL                              ((REG_READ(GPIO_IN1_REG)) & 0b10)
#define SCL_OR_SDA                       ((REG_READ(GPIO_IN1_REG)) & 0b11)
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA == 0b10)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA == 0b11)
#define IS_ACKNOWLEDGED                  '>'
#define NOT_ACKNOWLEDGED                 'x'

#define I2C_BUFFER_SIZE                  5000
#define UART_BUFFER_SIZE                 2000

#define WAIT_SDA_RISE                    while (SDA == 0)
#define WAIT_SDA_DROP                    while (SDA != 0)
#define WAIT_SCL_RISE                    while (SCL == 0)
#define WAIT_SCL_DROP                    while (SCL != 0)
#define WAIT_FOR_SDA_AND_SCL_RISE        while (SCL_OR_SDA != 0b11)
#define WAIT_FOR_SDA_OR_SCL_DROP         while (SCL_OR_SDA == 0b11)
#define WAIT_FOR_SDA_RISE_OR_SCL_DROP    while (SCL_OR_SDA == 0b10)

#define LENGTH_OF(A) (sizeof(A)/sizeof(*A))

SemaphoreHandle_t wifi_lock = NULL;
WiFiServer server(WIFI_DATA_PORT);
WiFiClient client;

volatile uint8_t b;
char str[I2C_BUFFER_SIZE];
volatile size_t str_len = 0;

char uart[UART_BUFFER_SIZE];
volatile size_t uart_len = 0;

void
print_data(const char *data, size_t data_len)
{
	if (xSemaphoreTake(wifi_lock, portMAX_DELAY) == pdTRUE) {
		if (client.connected()) {
			client.write(data, data_len);
		} else {
			client = server.available();
			if (client.connected()) {
				client.write(data, data_len);
			}
		}
		xSemaphoreGive(wifi_lock);
	}
}

const char *commands_list[] = {
	"1,RAC,1\r",
	"1,RSS,1\r",
	"1,RFI,1\r",
	"1,RAI,1\r",
	"1,RRC,1\r",
	"1,RI1,1\r",
};

void IRAM_ATTR
uart_loop(void *dummy)
{
	size_t i = 0;
	while (true) {
		Serial.write(commands_list[i]);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		if (Serial.available()) {
			uart_len = sprintf(uart, "\n#%lu=", millis());
			do {
				if (uart_len < UART_BUFFER_SIZE) {
					uart[uart_len++] = Serial.read();
					if (uart[uart_len - 1] == '\r') {
						uart[uart_len - 1] = '~';
					}
				}
			} while (Serial.available());
			print_data(uart, uart_len);
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
		i += 1;
		if (i >= LENGTH_OF(commands_list)) i = 0;
	}
}

void
setup(void)
{
	gpio_config_t cfg = {
		(1ULL << SDA_PIN) | (1ULL << SCL_PIN),
		GPIO_MODE_INPUT,
		GPIO_PULLUP_DISABLE,
		GPIO_PULLDOWN_DISABLE,
		GPIO_INTR_DISABLE,
	};
	gpio_config(&cfg);
	Serial.begin(SERIAL_SPEED);
	wifi_lock = xSemaphoreCreateMutex();
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED) {
		Serial.println("Not connected to Wi-Fi yet!");
		delay(1000);
	}
	server.begin();
	delay(1000);
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.macAddress());
	while (1) {
		client = server.available();
		Serial.println("Waiting for a client.");
		if (client.connected()) {
			Serial.println("Found a client!");
			break;
		}
		delay(500);
	}
	xTaskCreatePinnedToCore(
		&uart_loop,
		"uart_loop",
		4096,
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
loop(void)
{
i2c_sync:
	WAIT_FOR_SDA_AND_SCL_RISE;
i2c_idle:
	WAIT_FOR_SDA_OR_SCL_DROP;
	if (!SDA_IS_LOW_AND_SCL_IS_HIGH) goto i2c_sync;
i2c_start:
	str_len = sprintf(str, "\n%lu=", millis());
	WAIT_SCL_DROP;
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	str_len += sprintf(str + str_len, "%" PRIu8, b);
	if (SDA) {
		WAIT_SCL_DROP;
		str[str_len++] = NOT_ACKNOWLEDGED;
		print_data(str, str_len);
		goto i2c_sync;
	} else {
		WAIT_SCL_DROP;
		str[str_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
i2c_next:
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	str_len += sprintf(str + str_len, "%" PRIu8, b);
	if (SDA) {
		str[str_len++] = NOT_ACKNOWLEDGED;
	} else {
		str[str_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_DROP;

	while (SCL == 0) b = SDA;
	if (b) {
		WAIT_FOR_SDA_OR_SCL_DROP;
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) { // Repeated start signal.
			goto i2c_start;
		}
	} else {
		WAIT_FOR_SDA_RISE_OR_SCL_DROP;
		if (SDA_AND_SCL_ARE_HIGH) { // Stop signal.
			print_data(str, str_len);
			goto i2c_sync;
		}
	}

	b <<= 1;
	WAIT_SCL_DROP;
	goto i2c_next;
}
