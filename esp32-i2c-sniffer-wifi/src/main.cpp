#include <Arduino.h>
#include <WiFi.h>

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SERIAL_SPEED                     9600
#define WIFI_SSID                        "Prometheanica"
#define WIFI_PASSWORD                    "12345678"
#define WIFI_DATA_PORT                   80
#define SDA                              ((REG_READ(GPIO_IN1_REG)) & 0b01)
#define SCL                              ((REG_READ(GPIO_IN1_REG)) & 0b10)
#define SCL_OR_SDA                       ((REG_READ(GPIO_IN1_REG)) & 0b11)
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA == 0b10)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA == 0b11)
#define IS_ACKNOWLEDGED                  '>'
#define NOT_ACKNOWLEDGED                 'x'

#ifdef CONNECTION_IS_FLAKY
#define CHECK_FACTOR                     3
#define I2C_START_CONDITION_CHECK_FACTOR 3
#define I2C_START_CONDITION_PASS_FACTOR  2
#define I2C_SDA_CHECK_FACTOR             3
#define I2C_SDA_PASS_FACTOR              2
#define WAIT_SDA_RISE                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SDA == 0); }
#define WAIT_SDA_DROP                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SDA != 0); }
#define WAIT_SCL_RISE                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL == 0); }
#define WAIT_SCL_DROP                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL != 0); }
#define WAIT_FOR_SDA_AND_SCL_RISE        for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL_OR_SDA != 0b11); }
#define WAIT_FOR_SDA_OR_SCL_DROP         for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL_OR_SDA == 0b11); }
#define WAIT_FOR_SDA_RISE_OR_SCL_DROP    for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL_OR_SDA == 0b10); }
volatile int j, passes;
#else
#define WAIT_SDA_RISE                    while (SDA == 0)
#define WAIT_SDA_DROP                    while (SDA != 0)
#define WAIT_SCL_RISE                    while (SCL == 0)
#define WAIT_SCL_DROP                    while (SCL != 0)
#define WAIT_FOR_SDA_AND_SCL_RISE        while (SCL_OR_SDA != 0b11)
#define WAIT_FOR_SDA_OR_SCL_DROP         while (SCL_OR_SDA == 0b11)
#define WAIT_FOR_SDA_RISE_OR_SCL_DROP    while (SCL_OR_SDA == 0b10)
#endif

WiFiServer server(WIFI_DATA_PORT);
WiFiClient client;
volatile uint8_t b;
char str[10000];
volatile size_t str_len = 0;

uint8_t
i2c_start_condition(void)
{
#ifdef CONNECTION_IS_FLAKY
	passes = 0;
	for (j = 0; j < I2C_START_CONDITION_CHECK_FACTOR; ++j) {
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) {
			passes += 1;
		}
	}
	return !!(passes >= I2C_START_CONDITION_PASS_FACTOR);
#else
	return !!SDA_IS_LOW_AND_SCL_IS_HIGH;
#endif
}

uint8_t
read_sda(void)
{
#ifdef CONNECTION_IS_FLAKY
	passes = 0;
	for (j = 0; j < I2C_SDA_CHECK_FACTOR; ++j) {
		if (SDA) {
			passes += 1;
		}
	}
	return !!(passes >= I2C_SDA_PASS_FACTOR);
#else
	return SDA;
#endif
}

void
print_data(void)
{
	if (client.connected()) {
		client.write(str, str_len);
	} else {
		client = server.available();
		if (client.connected()) {
			client.write(str, str_len);
		}
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
	WiFi.mode(WIFI_AP);
	WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 8);
	server.begin();
	delay(1000);
	Serial.println(WiFi.softAPIP());
	while (1) {
		client = server.available();
		Serial.println("Waiting for a client.");
		if (client.connected()) {
			Serial.println("Found a client!");
			break;
		}
		delay(500);
	}
}

void
loop(void)
{
i2c_sync:
	WAIT_FOR_SDA_AND_SCL_RISE;
i2c_idle:
	WAIT_FOR_SDA_OR_SCL_DROP;
	if (!i2c_start_condition()) goto i2c_sync;
i2c_start:
	str_len = sprintf(str, "\n%lu=", millis());
	WAIT_SCL_DROP;
	WAIT_SCL_RISE; b  = read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda();          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	str_len += sprintf(str + str_len, "%" PRIu8, b);
	if (read_sda()) {
		WAIT_SCL_DROP;
		str[str_len++] = NOT_ACKNOWLEDGED;
		print_data();
		goto i2c_sync;
	} else {
		WAIT_SCL_DROP;
		str[str_len++] = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_RISE; b  = read_sda(); b <<= 1; WAIT_SCL_DROP;
i2c_next:
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda(); b <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; b |= read_sda();          WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	str_len += sprintf(str + str_len, "%" PRIu8, b);
	if (read_sda()) {
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
			print_data();
			goto i2c_sync;
		}
	}

	b <<= 1;
	WAIT_SCL_DROP;
	goto i2c_next;
}
