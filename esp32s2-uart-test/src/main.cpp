#include <Arduino.h>

#if 0
#define BEGIN_UART Serial1.begin(115200, SERIAL_8N1, 18, 16);
#define WRITE_UART Serial1.println("Hello, world!");
#else
#define BEGIN_UART Serial.begin(115200);
#define WRITE_UART Serial.println("Hello, world!");
#endif

void
setup()
{
	BEGIN_UART
	pinMode(15, OUTPUT);
	digitalWrite(15, HIGH);
}

void
loop()
{
	WRITE_UART
	delay(500);
}
