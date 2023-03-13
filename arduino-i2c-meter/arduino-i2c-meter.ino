#define SCL_FREQ_PIN 6 // When changing this, change bitmask below as well
#define SCL_VOLT_PIN A2
#define WAIT_FOR_DIGITAL_HIGH while ((PIND & 0b01000000) == 0)
#define WAIT_FOR_DIGITAL_LOW while ((PIND & 0b01000000) != 0)

size_t i;
int voltage, voltage_max;
unsigned long start_time, end_time;
double frequency;

void setup() {
	Serial.begin(9600);
	pinMode(SCL_FREQ_PIN, INPUT);
}

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
	Serial.println(voltage_max);
	// Measure frequency
	WAIT_FOR_DIGITAL_HIGH;
	WAIT_FOR_DIGITAL_LOW;
	start_time = micros();
	for (i = 0; i < 1000; ++i) {
		WAIT_FOR_DIGITAL_HIGH;
		WAIT_FOR_DIGITAL_LOW;
	}
	end_time = micros();
	frequency = 1000000000.0 / (end_time - start_time);
	Serial.print("Frequency is: ");
	Serial.println(frequency);
	// Rest a little
	delay(500);
}
