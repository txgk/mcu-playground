#include "main.h"
#include <stdlib.h>
#include <curses.h>

static WINDOW *status_window = NULL;
static char *message = NULL;
static size_t message_len = 0;
static size_t message_lim = 0;

static inline void
write_status_message_to_status_window(void)
{
	werase(status_window);
	mvwaddnstr(status_window, 0, 0, message == NULL ? "" : message, list_menu_width);
	wrefresh(status_window);
}

bool
status_recreate_unprotected(void)
{
	if (status_window != NULL) {
		delwin(status_window);
	}
	status_window = newwin(1, list_menu_width, list_menu_height, 0);
	if (status_window == NULL) {
		fputs("Failed to create status window!\n", stderr);
		return false;
	}
	write_status_message_to_status_window();
	return true;
}

void
status_write(const char *format, ...)
{
	pthread_mutex_lock(&interface_lock);
	// We need a copy of args because first call to vsnprintf screws original
	// argument list and we need to call vsnprintf after that.
	va_list args, args_copy;
	va_start(args, format);
	va_copy(args_copy, args);
	int required_length = vsnprintf(NULL, 0, format, args_copy);
	va_end(args_copy);
	if (required_length <= 0) {
		goto undo;
	}
	// We already know that integer is positive, so it is safe to cast it to unsigned integer.
	if ((size_t)required_length > message_lim) {
		message_lim = (size_t)required_length * 2 + 67;
		message = xrealloc(message, sizeof(char) * (message_lim + 1));
	}
	required_length = vsnprintf(message, required_length + 1, format, args);
	if (required_length < 0) {
		goto undo;
	}
	message_len = (size_t)required_length;
	message[message_len] = '\0';
	write_status_message_to_status_window();
undo:
	va_end(args);
	pthread_mutex_unlock(&interface_lock);
}

void
status_delete(void)
{
	free(message);
	delwin(status_window);
}
