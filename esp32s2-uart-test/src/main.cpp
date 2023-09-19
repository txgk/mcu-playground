#include <Arduino.h>

void
setup()
{
	Serial.begin(9600);
	pinMode(15, OUTPUT);
	digitalWrite(15, HIGH);
}

void
loop()
{
	Serial.println("Hello, world!");
	delay(500);
}
