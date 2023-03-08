#include "main.h"
#include <curses.h>

void
enter_interface(void)
{
	size_t selected_channel = 0;
	char input_str[1000];
	size_t input_len = 0;
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	while (1) {
		int c = getch();
		if (c == ' ') {
			print_analysis_results(selected_channel);
		} else if (ISDIGIT(c)) {
			input_str[input_len++] = c;
		} else if (c == '\n') {
			if (input_len != 0) {
				input_str[input_len] = '\0';
				input_len = 0;
				sscanf(input_str, "%zu", &selected_channel);
			}
			print_analysis_results(selected_channel);
		} else if (c == 'q') {
			break;
		}
	}
	endwin();
}
