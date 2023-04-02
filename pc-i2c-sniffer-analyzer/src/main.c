#include "main.h"
#include <string.h>

int
main(int argc, char **argv)
{
	int error = 0;

	const char *log_path = "/tmp/i2c-dumps/recent";
	if ((argc > 2) && (strcmp(argv[1], "-l") == 0)) {
		log_path = argv[2];
	}
	FILE *log_stream = fopen(log_path, "r");
	if (log_stream == NULL) {
		fprintf(stderr, "Failed to read %s\n", log_path);
		return 1;
	}

	if (!curses_init())                 { error = 2; goto undo1; }
	if (!adjust_list_menu())            { error = 3; goto undo2; }
	if (!status_recreate_unprotected()) { error = 4; goto undo3; }
	initialize_settings_of_list_menus();
	set_log_stream(log_stream);

	enter_overview_menu();

	status_delete();
undo3:
	free_list_menu();
undo2:
	curses_free();
undo1:
	fclose(log_stream);
	return error;
}
