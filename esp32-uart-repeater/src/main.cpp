#include <Arduino.h>

void
setup()
{
	Serial.begin(115200, SERIAL_8N1);
	Serial1.begin(115200, SERIAL_8N1, 16, 17);
}

void
loop()
{
	if (Serial1.available()) {
		Serial.write(Serial1.read());
	} else {
		delay(100);
	}
}
