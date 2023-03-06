#include <Wire.h>

char msg[100];
uint8_t address = 0;
uint8_t error;

void setup() {
	Serial.begin(9600);
	Wire.begin();
	/* Wire.setClock(60000L); */
}

void loop() {
	Wire.beginTransmission(address);
	delay(100);
	error = Wire.endTransmission();
	sprintf(msg, "Transmission to the device %3" PRIu8 " ended with an error code %" PRIu8 ".", address, error);
	Serial.println(msg);
	address = address == 127 ? 0 : address + 1;
	delay(100);
}
