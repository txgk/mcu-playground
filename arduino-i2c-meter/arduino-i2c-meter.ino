#define SCL_FREQ_PIN 6 // When changing this, change bitmask below as well
#define SCL_VOLT_PIN A2
#define WAIT_FOR_DIGITAL_HIGH while ((PIND & 0b01000000) == 0)
#define WAIT_FOR_DIGITAL_LOW while ((PIND & 0b01000000) != 0)

size_t i;
int voltage, voltage_max;
unsigned long start_time, end_time;
double frequency;
double frequency_avg = 0;
size_t frequency_count = 0;

void setup() {
	Serial.begin(9600);
	pinMode(SCL_FREQ_PIN, INPUT);
}

int periods_per_measure = 4;
int periods_cycles = 1000;

void loop() {
	// Measure voltage
	voltage_max = 0;
	for (i = 0; i < 1000; ++i) {
		voltage = analogRead(SCL_VOLT_PIN);
		if (voltage > voltage_max) {
			voltage_max = voltage;
		}
	}
	Serial.print("Voltage is: ");
	Serial.println(voltage_max * 5.0 / 1024);
	// Measure frequency
	frequency_count = 0;
	frequency_avg = 0;
	for (size_t i = 0; i < periods_cycles; ++i) {
		WAIT_FOR_DIGITAL_HIGH;
		WAIT_FOR_DIGITAL_LOW;
		start_time = micros();
		for (i = 0; i < periods_per_measure; ++i) {
			WAIT_FOR_DIGITAL_HIGH;
			WAIT_FOR_DIGITAL_LOW;
		}
		end_time = micros();
		frequency = periods_per_measure * 1000000.0 / (end_time - start_time);
		frequency_avg = frequency_avg * frequency_count + frequency;
		frequency_count += 1;
		frequency_avg /= frequency_count;
		Serial.print("Average frequency is: ");
		Serial.print(frequency_avg);
		Serial.print(" Current frequency is: ");
		Serial.println(frequency);
	}
	// Rest a little
	delay(500);
}
