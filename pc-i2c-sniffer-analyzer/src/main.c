#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static const char *
find_serial_terminal_to_device(void)
{
	const char *device_paths[] = {
		"/dev/ttyUSB0",
		"/dev/ttyACM0",
		"/dev/ttyUSB1",
		"/dev/ttyACM1",
	};
	for (size_t i = 0; i < 4; ++i) {
		if (access(device_paths[i], F_OK) == 0) {
			return device_paths[i];
		}
	}
	return NULL;
}

int
main(int argc, char **argv)
{
	int error = 0;
	const char *device_path;

	if ((argc > 2) && (strcmp(argv[1], "-d") == 0)) {
		device_path = argv[2];
	} else {
		device_path = find_serial_terminal_to_device();
	}
	if (device_path == NULL) {
		fprintf(stderr, "Failed to find serial terminal\n");
		return 1;
	}

	if (!start_serial_device_analysis(device_path, 115200)) { error = 1; goto undo0; }
	if (!curses_init())                                     { error = 2; goto undo1; }
	if (!adjust_list_menu())                                { error = 3; goto undo2; }
	initialize_settings_of_list_menus();

	enter_overview_menu();

	free_list_menu();
undo2:
	curses_free();
undo1:
	stop_serial_device_analysis();
undo0:
	return error;
}
