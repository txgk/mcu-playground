#include <Arduino.h>

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SDA                              ((REG_READ(GPIO_IN1_REG)) & 0b01)
#define SCL                              ((REG_READ(GPIO_IN1_REG)) & 0b10)
#define SDA_AND_SCL                      ((REG_READ(GPIO_IN1_REG)) & 0b11)
#define PACKETS_TO_STORE_BEFORE_PRINTING 10
#define CHECK_FACTOR                     30
#define I2C_START_CONDITION_CHECK_FACTOR 10
#define I2C_START_CONDITION_PASS_FACTOR  5
#define I2C_SDA_CHECK_FACTOR             10
#define I2C_SDA_PASS_FACTOR              5
#define WAIT_SDA_RISE                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SDA == 0); }
#define WAIT_SDA_DROP                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SDA != 0); }
#define WAIT_SCL_RISE                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL == 0); }
#define WAIT_SCL_DROP                    for (j = 0; j < CHECK_FACTOR; ++j) { while (SCL != 0); }
#define WAIT_FOR_BOTH_SDA_AND_SCL_HIGH   for (j = 0; j < CHECK_FACTOR; ++j) { while (SDA_AND_SCL != 0b11); }
#define WAIT_FOR_SDA_OR_SCL_DROP         for (j = 0; j < CHECK_FACTOR; ++j) { while (SDA_AND_SCL == 0b11); }
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SDA_AND_SCL == 0b10)
#define BYTE_SEPARATOR                   256
#define PACKET_SEPARATOR                 512

volatile uint16_t d[10000];
volatile uint16_t *p = d;
volatile uint16_t *ptr;
volatile int j;
volatile int packets = 0;
volatile int passes;

gpio_config_t cfg = {
	(1ULL << SDA_PIN) | (1ULL << SCL_PIN),
	GPIO_MODE_INPUT,
	GPIO_PULLUP_ENABLE,
	GPIO_PULLDOWN_DISABLE,
	GPIO_INTR_DISABLE,
};

static void
print_data(void)
{
	for (ptr = d; ptr < p; ptr += 1) {
		if (*ptr == BYTE_SEPARATOR) {
			Serial.print(':');
		} else if (*ptr == PACKET_SEPARATOR) {
			Serial.print('\n');
		} else {
#if 0
			if (*ptr < 16) {
				Serial.print('0');
			}
			Serial.print(*ptr, HEX);
#else
			Serial.print((char)*ptr);
#endif
		}
	}
	Serial.flush();
	p = d;
	packets = 0;
}

static uint8_t
i2c_start_condition(void)
{
	passes = 0;
	for (j = 0; j < I2C_START_CONDITION_CHECK_FACTOR; ++j) {
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) {
			passes += 1;
		}
	}
	return passes >= I2C_START_CONDITION_PASS_FACTOR ? 1 : 0;
}

static uint8_t
get_sda(void)
{
	passes = 0;
	for (j = 0; j < I2C_SDA_CHECK_FACTOR; ++j) {
		if (SDA) {
			passes += 1;
		}
	}
	return passes >= I2C_SDA_PASS_FACTOR ? 1 : 0;
}

void
setup(void)
{
	Serial.begin(460800);
	gpio_config(&cfg);
}

void
loop(void)
{
i2c_sync:
	WAIT_FOR_BOTH_SDA_AND_SCL_HIGH;
i2c_idle:
	WAIT_FOR_SDA_OR_SCL_DROP;
	if (!i2c_start_condition()) goto i2c_sync;
	WAIT_SCL_DROP;
i2c_start:
	WAIT_SCL_RISE; *p  = get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); p += 1;   WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	if (get_sda()) {
		WAIT_SCL_DROP;
		*p++ = PACKET_SEPARATOR;
		packets += 1;
		if (packets >= PACKETS_TO_STORE_BEFORE_PRINTING) {
			print_data();
		}
		goto i2c_sync;
	} else {
		WAIT_SCL_DROP;
		*p++ = BYTE_SEPARATOR;
	}
i2c_next:
	WAIT_SCL_RISE; *p  = get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= get_sda(); p += 1;   WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	if (get_sda()) {
		WAIT_SCL_DROP;
		*p++ = PACKET_SEPARATOR;
		packets += 1;
		if (packets >= PACKETS_TO_STORE_BEFORE_PRINTING) {
			print_data();
		}
		goto i2c_sync;
	} else {
		WAIT_SCL_DROP;
		*p++ = BYTE_SEPARATOR;
		goto i2c_next;
	}
}
