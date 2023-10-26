#include <Arduino.h>

#define INPUT_SIZE 5
char input[INPUT_SIZE];

void
setup()
{
	Serial.begin(115200, SERIAL_8N1);
}

void
loop()
{
	size_t input_len = 0;
	while (Serial.available()) {
		int data = Serial.read();
		if (data < 0) break;
		if (input_len < INPUT_SIZE) {
			input[input_len++] = data;
		}
	}
	if (input_len == 5
		&& input[0] == 'm'
		&& input[1] == 'a'
		&& input[2] == 'r'
		&& input[3] == 'c'
		&& input[4] == 'o')
	{
		Serial.write("polo");
	}
	delay(500);
}
