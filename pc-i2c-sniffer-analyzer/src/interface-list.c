#include "main.h"
#include <stdlib.h>
#include <curses.h>

struct list_menu_settings {
	size_t entries_count; // Total number of entries.
	size_t view_sel; // Index of the selected entry.
	size_t view_min; // Index of the first visible entry.
	size_t view_max; // Index of the last visible entry.
	const char *(*write)(size_t index);
};

size_t list_menu_height;
size_t list_menu_width;

static WINDOW **windows = NULL;
static size_t windows_count = 0;
static const size_t scrolloff = 0;

static struct list_menu_settings menus[MENUS_COUNT];
static struct list_menu_settings *menu = menus; // Selected menu.

bool
adjust_list_menu(void)
{
	// Delete old windows to create new windows with the new size because
	// useful in this case resizing is a non-standard curses extension.
	for (size_t i = 0; i < windows_count; ++i) {
		delwin(windows[i]);
	}
	windows = xrealloc(windows, sizeof(WINDOW *) * list_menu_height);
	for (size_t i = 0; i < list_menu_height; ++i) {
		windows[i] = newwin(1, list_menu_width, i, 0);
		if (windows[i] == NULL) {
			complain_about_insufficient_memory_and_exit();
			return false;
		}
	}
	windows_count = list_menu_height;
	return true;
}

void
free_list_menu(void)
{
	for (size_t i = 0; i < windows_count; ++i) {
		delwin(windows[i]);
	}
	free(windows);
}

void
initialize_settings_of_list_menus(void)
{
	menus[OVERVIEW_MENU].write = &print_overview_menu_entry;
	menus[SAMPLES_MENU].write  = &print_samples_menu_entry;
}

static inline void
expose_entry_of_the_list_menu_unprotected(size_t index)
{
	WINDOW *w = windows[index - menu->view_min];
	werase(w);
	mvwaddnstr(w, 0, 0, menu->write(index), list_menu_width);
	wbkgd(w, index == menu->view_sel ? A_REVERSE : A_NORMAL);
	wrefresh(w);
}

void
expose_entry_of_the_list_menu(size_t index)
{
	pthread_mutex_lock(&interface_lock);
	if ((index >= menu->view_min) && (index <= menu->view_max)) {
		expose_entry_of_the_list_menu_unprotected(index);
	}
	pthread_mutex_unlock(&interface_lock);
}

static inline void
expose_all_visible_entries_of_the_list_menu_unprotected(void)
{
	for (size_t i = menu->view_min; i <= menu->view_max && i < menu->entries_count; ++i) {
		expose_entry_of_the_list_menu_unprotected(i);
	}
}

void
expose_all_visible_entries_of_the_list_menu(void)
{
	pthread_mutex_lock(&interface_lock);
	expose_all_visible_entries_of_the_list_menu_unprotected();
	pthread_mutex_unlock(&interface_lock);
}

static inline void
erase_all_visible_entries_not_in_the_list_menu_unprotected(void)
{
	for (size_t i = menu->entries_count; i <= menu->view_max; ++i) {
		werase(windows[i - menu->view_min]);
		wbkgd(windows[i - menu->view_min], A_NORMAL);
		wnoutrefresh(windows[i - menu->view_min]);
	}
	doupdate();
}

void
redraw_list_menu_unprotected(void)
{
	menu->view_max = menu->view_min + (list_menu_height - 1);
	if (menu->view_sel > menu->view_max) {
		menu->view_max = menu->view_sel;
		menu->view_min = menu->view_max - (list_menu_height - 1);
	}
	expose_all_visible_entries_of_the_list_menu_unprotected();
	erase_all_visible_entries_not_in_the_list_menu_unprotected();
}

const size_t *
enter_list_menu(int8_t menu_index, size_t new_entries_count)
{
	pthread_mutex_lock(&interface_lock);
	menu = menus + menu_index;
	menu->entries_count = new_entries_count;
	menu->view_sel = 0;
	menu->view_min = 0;
	redraw_list_menu_unprotected();
	pthread_mutex_unlock(&interface_lock);
	return &(menu->view_sel);
}

void
reset_list_menu(size_t new_entries_count)
{
	pthread_mutex_lock(&interface_lock);
	if (new_entries_count < menu->entries_count) {
		menu->view_sel = 0;
		menu->view_min = 0;
	}
	menu->entries_count = new_entries_count;
	redraw_list_menu_unprotected();
	pthread_mutex_unlock(&interface_lock);
}

void
leave_list_menu(void)
{
	pthread_mutex_lock(&interface_lock);
	menu = menus + OVERVIEW_MENU;
	redraw_list_menu_unprotected();
	pthread_mutex_unlock(&interface_lock);
}

static void
list_menu_change_view(size_t new_sel)
{
	// Gotta lock right away because we use menu->entries_count below.
	pthread_mutex_lock(&interface_lock);

	if (new_sel >= menu->entries_count) {
		if (menu->entries_count == 0) {
			pthread_mutex_unlock(&interface_lock);
			return;
		}
		new_sel = menu->entries_count - 1;
	}

	if ((new_sel + scrolloff) > menu->view_max) {
		if (menu->entries_count > list_menu_height) {
			menu->view_max = MIN(new_sel + scrolloff, menu->entries_count - 1);
		} else {
			menu->view_max = list_menu_height - 1;
		}
		menu->view_min = menu->view_max - (list_menu_height - 1);
		menu->view_sel = new_sel;
		expose_all_visible_entries_of_the_list_menu_unprotected();
	} else if ((new_sel >= scrolloff) && ((new_sel - scrolloff) < menu->view_min)) {
		menu->view_min = new_sel - scrolloff;
		menu->view_max = menu->view_min + (list_menu_height - 1);
		menu->view_sel = new_sel;
		expose_all_visible_entries_of_the_list_menu_unprotected();
	} else if (new_sel < menu->view_min) {
		menu->view_min = new_sel < scrolloff ? 0 : new_sel - scrolloff;
		menu->view_max = menu->view_min + (list_menu_height - 1);
		menu->view_sel = new_sel;
		expose_all_visible_entries_of_the_list_menu_unprotected();
	} else if (new_sel != menu->view_sel) {
		WINDOW *w = windows[menu->view_sel - menu->view_min];
		wbkgd(w, A_NORMAL);
		wrefresh(w);
		menu->view_sel = new_sel;
		w = windows[menu->view_sel - menu->view_min];
		wbkgd(w, A_REVERSE);
		wrefresh(w);
	}

	pthread_mutex_unlock(&interface_lock);
}

bool
handle_list_menu_navigation(int c)
{
	if (c == KEY_DOWN) {
		list_menu_change_view(menu->view_sel + 1);
	} else if (c == KEY_UP) {
		list_menu_change_view(menu->view_sel > 1 ? (menu->view_sel - 1) : 0);
	} else if (c == 'g') {
		list_menu_change_view(0);
	} else if (c == 'G') {
		list_menu_change_view(menu->entries_count > 1 ? (menu->entries_count - 1) : 0);
	} else {
		return false;
	}
	return true;
}
