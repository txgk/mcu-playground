#include <Arduino.h>
#include <WiFi.h>
#include "wifi-credentials.h"

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SDA2_PIN                         34
#define SCL2_PIN                         35
#define SERIAL_SPEED                     115200
#define WIFI_DATA_PORT                   80
#define SDA                              ((REG_READ(GPIO_IN1_REG)) & 0b0001)
#define SCL                              ((REG_READ(GPIO_IN1_REG)) & 0b0010)
#define SDA2                             ((REG_READ(GPIO_IN1_REG)) & 0b0100)
#define SCL2                             ((REG_READ(GPIO_IN1_REG)) & 0b1000)
#define SCL_OR_SDA                       ((REG_READ(GPIO_IN1_REG)) & 0b0011)
#define SCL2_OR_SDA2                     ((REG_READ(GPIO_IN1_REG)) & 0b1100)
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA   == 0b0010)
#define SDA2_IS_LOW_AND_SCL2_IS_HIGH     (SCL2_OR_SDA2 == 0b1000)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA   == 0b0011)
#define SDA2_AND_SCL2_ARE_HIGH           (SCL2_OR_SDA2 == 0b1100)
#define IS_ACKNOWLEDGED                  '>'
#define NOT_ACKNOWLEDGED                 'x'

#define WAIT_SDA_RISE                    while (SDA  == 0)
#define WAIT_SDA2_RISE                   while (SDA2 == 0)
#define WAIT_SDA_DROP                    while (SDA  != 0)
#define WAIT_SDA2_DROP                   while (SDA2 != 0)
#define WAIT_SCL_RISE                    while (SCL  == 0)
#define WAIT_SCL2_RISE                   while (SCL2 == 0)
#define WAIT_SCL_DROP                    while (SCL  != 0)
#define WAIT_SCL2_DROP                   while (SCL2 != 0)
#define WAIT_FOR_SDA_AND_SCL_RISE        while (SCL_OR_SDA   != 0b0011)
#define WAIT_FOR_SDA2_AND_SCL2_RISE      while (SCL2_OR_SDA2 != 0b1100)
#define WAIT_FOR_SDA_OR_SCL_DROP         while (SCL_OR_SDA   == 0b0011)
#define WAIT_FOR_SDA2_OR_SCL2_DROP       while (SCL2_OR_SDA2 == 0b1100)
#define WAIT_FOR_SDA_RISE_OR_SCL_DROP    while (SCL_OR_SDA   == 0b0010)
#define WAIT_FOR_SDA2_RISE_OR_SCL2_DROP  while (SCL2_OR_SDA2 == 0b1000)

#define I2C1_BUFFER_SIZE                 4000
#define I2C2_BUFFER_SIZE                 4000
#define UART_BUFFER_SIZE                 1000

#define LENGTH_OF(A)                     (sizeof(A)/sizeof(*A))

SemaphoreHandle_t wifi_lock = NULL;
WiFiServer server(WIFI_DATA_PORT);
WiFiClient client;

volatile uint8_t b, c;
char i2c1[I2C1_BUFFER_SIZE], i2c2[I2C2_BUFFER_SIZE];
volatile size_t i2c1_len = 0, i2c2_len = 0;

char uart[UART_BUFFER_SIZE];
volatile size_t uart_len = 0;
const char *uart_commands[] = {
	"1,RAC,1\r",
	"1,RSS,1\r",
	"1,RFI,1\r",
	"1,RAI,1\r",
	"1,RRC,1\r",
	"1,RI1,1\r",
};

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

void IRAM_ATTR
i2c1_handler(void *dummy)
{
i2c1_sync:
	WAIT_FOR_SDA_AND_SCL_RISE;
i2c1_idle:
	WAIT_FOR_SDA_OR_SCL_DROP;
	if (!SDA_IS_LOW_AND_SCL_IS_HIGH) goto i2c1_sync;
i2c1_start:
	i2c1_len = sprintf(i2c1, "\nI2C1@%lu=", millis());
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
	i2c1_len += sprintf(i2c1 + i2c1_len, "%" PRIu8, b);
	if (SDA) {
		WAIT_SCL_DROP;
		i2c1[i2c1_len++] = NOT_ACKNOWLEDGED;
		print_data(i2c1, i2c1_len);
		goto i2c1_sync;
	} else {
		WAIT_SCL_DROP;
		i2c1[i2c1_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_RISE; b  = SDA; b <<= 1; WAIT_SCL_DROP;
i2c1_next:
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA; b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= SDA;          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	i2c1_len += sprintf(i2c1 + i2c1_len, "%" PRIu8, b);
	if (SDA) {
		i2c1[i2c1_len++] = NOT_ACKNOWLEDGED;
	} else {
		i2c1[i2c1_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_DROP;

	while (SCL == 0) b = SDA;
	if (b) {
		WAIT_FOR_SDA_OR_SCL_DROP;
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) { // Repeated start signal.
			goto i2c1_start;
		}
	} else {
		WAIT_FOR_SDA_RISE_OR_SCL_DROP;
		if (SDA_AND_SCL_ARE_HIGH) { // Stop signal.
			print_data(i2c1, i2c1_len);
			goto i2c1_sync;
		}
	}

	b <<= 1;
	WAIT_SCL_DROP;
	goto i2c1_next;
}

void IRAM_ATTR
i2c2_handler(void *dummy)
{
i2c2_sync:
	WAIT_FOR_SDA2_AND_SCL2_RISE;
i2c2_idle:
	WAIT_FOR_SDA2_OR_SCL2_DROP;
	if (!SDA2_IS_LOW_AND_SCL2_IS_HIGH) goto i2c2_sync;
i2c2_start:
	i2c2_len = sprintf(i2c2, "\nI2C2@%lu=", millis());
	WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c  = SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2;          WAIT_SCL2_DROP;
	WAIT_SCL2_RISE;
	i2c2_len += sprintf(i2c2 + i2c2_len, "%" PRIu8, c);
	if (SDA2) {
		WAIT_SCL2_DROP;
		i2c2[i2c2_len++] = NOT_ACKNOWLEDGED;
		print_data(i2c2, i2c2_len);
		goto i2c2_sync;
	} else {
		WAIT_SCL2_DROP;
		i2c2[i2c2_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL2_RISE; c  = SDA2; c <<= 1; WAIT_SCL2_DROP;
i2c2_next:
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2; c <<= 1; WAIT_SCL2_DROP;
	WAIT_SCL2_RISE; c |= SDA2;          WAIT_SCL2_DROP;
	WAIT_SCL2_RISE;
	i2c2_len += sprintf(i2c2 + i2c2_len, "%" PRIu8, c);
	if (SDA2) {
		i2c2[i2c2_len++] = NOT_ACKNOWLEDGED;
	} else {
		i2c2[i2c2_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL2_DROP;

	while (SCL2 == 0) c = SDA2;
	if (c) {
		WAIT_FOR_SDA2_OR_SCL2_DROP;
		if (SDA2_IS_LOW_AND_SCL2_IS_HIGH) { // Repeated start signal.
			goto i2c2_start;
		}
	} else {
		WAIT_FOR_SDA2_RISE_OR_SCL2_DROP;
		if (SDA2_AND_SCL2_ARE_HIGH) { // Stop signal.
			print_data(i2c2, i2c2_len);
			goto i2c2_sync;
		}
	}

	c <<= 1;
	WAIT_SCL2_DROP;
	goto i2c2_next;
}

void IRAM_ATTR
uart_loop(void *dummy)
{
	size_t i = 0;
	while (true) {
		Serial.write(uart_commands[i]);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		if (Serial.available()) {
			uart_len = sprintf(uart, "\nUART@%lu=", millis());
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
		if (i >= LENGTH_OF(uart_commands)) i = 0;
	}
}

void
setup(void)
{
	gpio_config_t cfg = {
		(1ULL << SDA_PIN) | (1ULL << SCL_PIN) | (1ULL << SDA2_PIN) | (1ULL << SCL2_PIN),
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
		&i2c1_handler,
		"i2c1_handler",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
	xTaskCreatePinnedToCore(
		&i2c2_handler,
		"i2c2_handler",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
	xTaskCreatePinnedToCore(
		&uart_loop,
		"uart_loop",
		2048, // stack size
		NULL, // argument
		1, // priority
		NULL, // handle
		1 // core
	);
}

void
loop(void)
{
	delay(100);
}
