#include <Wire.h>

char msg[100];
uint8_t address = 0;
uint8_t error;

void setup() {
	Serial.begin(9600);
}

void loop() {
	sprintf(msg, "Listening %d...", address);
	Serial.print(msg);
	Wire.begin(address);
	/* Wire.setClock(60000L); */
	delay(500);
	while (Wire.available()) {
		Serial.print(' ');
		Serial.print(Wire.read());
	}
	Serial.print('\n');
	Wire.end();
	address = address == 127 ? 0 : address + 1;
	delay(10);
}
