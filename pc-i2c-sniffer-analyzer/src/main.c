#include "main.h"

int
main(void)
{
	if (start_serial_device_analysis(NULL, 115200) == false) {
		return 1;
	}
	enter_interface();
	stop_serial_device_analysis();
	return 0;
}
