#include "main.h"
#include <curses.h>

pthread_mutex_t interface_lock = PTHREAD_MUTEX_INITIALIZER;

static bool
obtain_terminal_size(void)
{
	int terminal_width = getmaxx(stdscr);
	if (terminal_width == ERR) {
		fputs("Failed to get width of the terminal!\n", stderr);
		return false;
	}
	if (terminal_width < 50) {
		fputs("Terminal is too narrow!\n", stderr);
		return false;
	}
	int terminal_height = getmaxy(stdscr);
	if (terminal_height == ERR) {
		fputs("Failed to get height of the terminal!\n", stderr);
		return false;
	}
	if (terminal_height < 5) {
		fputs("Terminal is too short!\n", stderr);
		return false;
	}

	list_menu_width = terminal_width;
	list_menu_height = terminal_height - 1;

	return true;
}

bool
curses_init(void)
{
	if (initscr() == NULL) {
		fputs("Initialization of curses data structures failed!\n", stderr);
		return false;
	}
	if (cbreak() == ERR) {
		fputs("Can not disable line buffering and erase/kill character-processing!\n", stderr);
		return false;
	}
	if (obtain_terminal_size() == false) {
		fputs("Invalid terminal size obtained!\n", stderr);
		return false;
	}
	timeout(0);
	curs_set(0);
	noecho();
	keypad(stdscr, TRUE);
	return true;
}

void
curses_free(void)
{
	endwin();
}

bool
resize_counter_action(void)
{
	pthread_mutex_lock(&interface_lock);

	// We need to call clear and refresh before all resize actions because
	// the junk text of previous size may remain in inactive areas.
	clear();
	refresh();

	if (obtain_terminal_size() == false) {
		// Some really crazy resize happend. It is either a glitch or user
		// deliberately trying to break something. This state is unusable anyways.
		fputs("Don't flex around with me, okay?\n", stderr);
		goto error;
	}
	if (adjust_list_menu() == false) {
		goto error;
	}
	redraw_list_menu_unprotected();
	pthread_mutex_unlock(&interface_lock);
	return true;
error:
	pthread_mutex_unlock(&interface_lock);
	return false;
}
