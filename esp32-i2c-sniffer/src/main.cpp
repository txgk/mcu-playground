#include <Arduino.h>

#define SDA_PIN                          32
#define SCL_PIN                          33
#define SERIAL_SPEED                     115200
#define SDA                              ((REG_READ(GPIO_IN1_REG)) & 0b01)
#define SCL                              ((REG_READ(GPIO_IN1_REG)) & 0b10)
#define SCL_OR_SDA                       ((REG_READ(GPIO_IN1_REG)) & 0b11)
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA == 0b10)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA == 0b11)
#define IS_ACKNOWLEDGED                  256
#define NOT_ACKNOWLEDGED                 512
#define MESSAGE_SEPARATOR                1024

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

volatile uint16_t d[10000];
volatile uint16_t *p = d;
volatile uint16_t *ptr;
volatile unsigned long timestamp;

gpio_config_t cfg = {
	(1ULL << SDA_PIN) | (1ULL << SCL_PIN),
	GPIO_MODE_INPUT,
	GPIO_PULLUP_DISABLE,
	GPIO_PULLDOWN_DISABLE,
	GPIO_INTR_DISABLE,
};

void
print_data(void)
{
	for (ptr = d; ptr < p; ptr += 1) {
		if (*ptr == IS_ACKNOWLEDGED) {
			Serial.print('>');
		} else if (*ptr == MESSAGE_SEPARATOR) {
			Serial.print('\n');
			Serial.print(timestamp);
			Serial.print('=');
		} else if (*ptr == NOT_ACKNOWLEDGED) {
			Serial.print('x');
		} else {
			Serial.print(*ptr);
		}
	}
	Serial.flush();
	p = d;
}

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
setup(void)
{
	Serial.begin(SERIAL_SPEED);
	gpio_config(&cfg);
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
	*p++ = MESSAGE_SEPARATOR;
	timestamp = millis();
	WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p  = read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); p += 1;   WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	if (read_sda()) {
		WAIT_SCL_DROP;
		*p++ = NOT_ACKNOWLEDGED;
		print_data();
		goto i2c_sync;
	} else {
		WAIT_SCL_DROP;
		*p++ = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_RISE; *p  = read_sda(); *p <<= 1; WAIT_SCL_DROP;
i2c_next:
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); *p <<= 1; WAIT_SCL_DROP;
	WAIT_SCL_RISE; *p |= read_sda(); p += 1;   WAIT_SCL_DROP;
	WAIT_SCL_RISE;
	if (read_sda()) {
		*p++ = NOT_ACKNOWLEDGED;
	} else {
		*p++ = IS_ACKNOWLEDGED;
	}
	WAIT_SCL_DROP;

	while (SCL == 0) *p = SDA;
	if (*p) {
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

	*p <<= 1;
	WAIT_SCL_DROP;
	goto i2c_next;
}
