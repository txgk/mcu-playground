#define SDA_PIN                          6
#define SCL_PIN                          7
#define SDA                              digitalReadDirect(SDA_PIN)
#define SCL                              digitalReadDirect(SCL_PIN)
#define SCL_OR_SDA                       ((digitalReadDirect(SCL_PIN) << 1) | digitalReadDirect(SDA_PIN))
#define SDA_IS_LOW_AND_SCL_IS_HIGH       (SCL_OR_SDA == 0b10)
#define SDA_AND_SCL_ARE_HIGH             (SCL_OR_SDA == 0b11)
#define PACKETS_TO_STORE_BEFORE_PRINTING 10
#define BYTE_SEPARATOR                   256
#define MESSAGE_SEPARATOR                512
#define PACKET_SEPARATOR                 1024

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
volatile int packets = 0;

inline uint8_t
digitalReadDirect(int pin)
{
	return !!(g_APinDescription[pin].pPort->PIO_PDSR & g_APinDescription[pin].ulPin);
}

void
print_data(void)
{
	for (ptr = d; ptr < p; ptr += 1) {
		if (*ptr == BYTE_SEPARATOR) {
			Serial.print(':');
		} else if (*ptr == MESSAGE_SEPARATOR) {
			Serial.print('\n');
		} else if (*ptr == PACKET_SEPARATOR) {
			Serial.print('$');
		} else {
#if 1
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
	Serial.begin(115200);
	pinMode(SDA_PIN, INPUT);
	pinMode(SCL_PIN, INPUT);
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
		*p++ = PACKET_SEPARATOR;
	} else {
		*p++ = BYTE_SEPARATOR;
	}
	WAIT_SCL_DROP;

	while (SCL == 0) *p = SDA;
	if (*p) {
		WAIT_FOR_SDA_OR_SCL_DROP;
		if (SDA_IS_LOW_AND_SCL_IS_HIGH) {
			goto i2c_start;
		}
	} else {
		WAIT_FOR_SDA_RISE_OR_SCL_DROP;
		if (SDA_AND_SCL_ARE_HIGH) {
			packets += 1;
			if (packets >= PACKETS_TO_STORE_BEFORE_PRINTING) {
				print_data();
			}
			goto i2c_sync;
		}
	}

	*p <<= 1;
	WAIT_SCL_DROP;
	goto i2c_next;
}
