#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
	int error = 0;

	const char *log_path = "/tmp/i2c-dumps/recent";
	if ((argc > 2) && (strcmp(argv[1], "-l") == 0)) {
		log_path = argv[2];
	}

	if (!start_i2c_log_analysis(log_path)) { error = 1; goto undo0; }
	if (!curses_init())                    { error = 2; goto undo1; }
	if (!adjust_list_menu())               { error = 3; goto undo2; }
	initialize_settings_of_list_menus();

	enter_overview_menu();

	free_list_menu();
undo2:
	curses_free();
undo1:
	stop_i2c_log_analysis();
undo0:
	return error;
}
