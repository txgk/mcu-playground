#define SDA_PIN                          6
#define SCL_PIN                          7
#define SERIAL_SPEED                     115200
#define SDA                              digitalReadDirect(SDA_PIN)
#define SCL                              digitalReadDirect(SCL_PIN)
#define SCL_OR_SDA                       ((digitalReadDirect(SCL_PIN) << 1) | digitalReadDirect(SDA_PIN))
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

volatile uint8_t b;
char str[10000];
volatile size_t str_len = 0;

inline uint8_t
digitalReadDirect(int pin)
{
	return !!(g_APinDescription[pin].pPort->PIO_PDSR & g_APinDescription[pin].ulPin);
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
print_data(void)
{
	Serial.write(str, str_len);
	Serial.flush();
}

void
setup(void)
{
	pinMode(SDA_PIN, INPUT);
	pinMode(SCL_PIN, INPUT);
	Serial.begin(SERIAL_SPEED);
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
