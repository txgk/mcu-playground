#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <math.h>

#define INITIAL_PACKET_SIZE 5000

struct i2c_packet_sample {
	struct i2c_packet *packet;
	size_t packets_count;
	char *packet_string;
	long double period_ms;
	long double frequency_hz;
};

struct node_data {
	int address;
	size_t packets_count;
	size_t reads_count;
	size_t writes_count;
	struct i2c_packet_sample *samples;
	size_t samples_count;
	size_t min_packet_size;
	size_t max_packet_size;
	double avg_packet_size;
	struct timespec start_time;
	long double period_ms;
	long double frequency_hz;
};

static FILE *device_stream = NULL;
static pthread_t device_handler_thread;
static volatile bool device_handler_must_finish = false;
static struct node_data *nodes = NULL;
static size_t nodes_count = 0;
static struct node_data *selected_node = NULL;
static char entry_content_buffer[5000];
static bool in_samples_menu = false;
static const struct timespec input_delay = {0, 10000000};

static bool
setup_serial_device(const char *device_path, long baud_rate)
{
	// To make device terminal work as serial stream we have to setup its
	// characteristics properly.
	char setup_command[100];
	char baud_rate_string[50];
	sprintf(baud_rate_string, "%ld", baud_rate);
	strcpy(setup_command, "stty -F ");
	strcat(setup_command, device_path);
	strcat(setup_command, " ");
	strcat(setup_command, baud_rate_string);
	strcat(setup_command, " raw crtscts -echo");
	return system(setup_command) == 0 ? true : false;
}

static size_t
make_sure_node_exists(int address)
{
	for (size_t i = 0; i < nodes_count; ++i) {
		if (address == nodes[i].address) {
			return i;
		}
	}
	nodes = xrealloc(nodes, sizeof(struct node_data) * (nodes_count + 1));
	nodes[nodes_count].address = address;
	nodes[nodes_count].packets_count = 0;
	nodes[nodes_count].reads_count = 0;
	nodes[nodes_count].writes_count = 0;
	nodes[nodes_count].samples = NULL;
	nodes[nodes_count].samples_count = 0;
	nodes[nodes_count].min_packet_size = 999999;
	nodes[nodes_count].max_packet_size = 0;
	nodes[nodes_count].avg_packet_size = 0;
	clock_gettime(CLOCK_REALTIME, &nodes[nodes_count].start_time);
	nodes[nodes_count].period_ms = INFINITY;
	nodes[nodes_count].frequency_hz = 0;
	nodes_count += 1;
	return nodes_count - 1;
}

static void
assign_packet_to_node(struct node_data *node, struct i2c_packet *packet)
{
	if (packet->is_read == true) {
		node->reads_count += 1;
	} else {
		node->writes_count += 1;
	}
	if (packet->data_len < node->min_packet_size) {
		node->min_packet_size = packet->data_len;
	}
	if (packet->data_len > node->max_packet_size) {
		node->max_packet_size = packet->data_len;
	}
	node->avg_packet_size = node->avg_packet_size * node->packets_count + packet->data_len;
	node->packets_count += 1;
	node->avg_packet_size /= node->packets_count;
	node->period_ms = ((long double)packet->gettime.tv_sec - node->start_time.tv_sec + ((long double)packet->gettime.tv_nsec - node->start_time.tv_nsec) / 1000000000.0L) / node->packets_count * 1000.0L;
	node->frequency_hz = 1000.0L / node->period_ms;

	for (size_t i = 0; i < node->samples_count; ++i) {
		if (i2c_packets_are_equal(packet, node->samples[i].packet)) {
			struct i2c_packet_sample *sample = node->samples + i;
			sample->packets_count += 1;
			sample->period_ms = ((long double)packet->gettime.tv_sec - sample->packet->gettime.tv_sec + ((long double)packet->gettime.tv_nsec - sample->packet->gettime.tv_nsec) / 1000000000.0L) / sample->packets_count * 1000.0L;
			sample->frequency_hz = 1000.0L / sample->period_ms;
			free_i2c_packet(packet);
			return;
		}
	}

	node->samples = xrealloc(node->samples,
		sizeof(struct i2c_packet_sample) * (node->samples_count + 1));
	node->samples[node->samples_count].packet = packet;
	node->samples[node->samples_count].packets_count = 1;
	node->samples[node->samples_count].packet_string = i2c_packet_convert_to_string(packet);
	node->samples[node->samples_count].period_ms = INFINITY;
	node->samples[node->samples_count].frequency_hz = 0;
	node->samples_count += 1;
}

static void *
device_handler(void *dummy)
{
	(void)dummy;
	char c;
	char *packet = xmalloc(sizeof(char) * INITIAL_PACKET_SIZE);
	size_t packet_len = 0;
	size_t packet_lim = INITIAL_PACKET_SIZE;
	struct timespec current_time;
	struct timespec last_get_time = {0, 0};
	struct i2c_packet *parsed_packet;
	size_t node_index;
	while (device_handler_must_finish == false) {
		c = fgetc(device_stream);
		if (c == '\n') {
			if ((packet_len < 3) || ((packet[0] != 'r') && (packet[0] != 'w'))) {
				packet_len = 0;
				continue;
			}
			parsed_packet = i2c_packet_parse(packet, packet_len);
			pthread_mutex_lock(&interface_lock);
			node_index = make_sure_node_exists(parsed_packet->data[0]);
			assign_packet_to_node(nodes + node_index, parsed_packet);
			clock_gettime(CLOCK_REALTIME, &current_time);
			if ((current_time.tv_sec - last_get_time.tv_sec > 0) || (current_time.tv_nsec - last_get_time.tv_nsec > 200000000))
			{
				if (in_samples_menu == true) {
					if (selected_node != NULL) {
						reset_list_menu_unprotected(selected_node->packets_count + 3);
					}
				} else {
					reset_list_menu_unprotected(nodes_count + 3);
				}
				last_get_time.tv_sec = current_time.tv_sec;
				last_get_time.tv_nsec = current_time.tv_nsec;
			}
			pthread_mutex_unlock(&interface_lock);
			packet_len = 0;
		} else if (c == EOF) {
			break;
		} else {
			if (packet_len >= packet_lim) {
				packet_lim = packet_lim * 2 + 67;
				packet = xrealloc(packet, sizeof(char) * packet_lim);
			}
			packet[packet_len++] = c;
		}
	}
	free(packet);
	return NULL;
}

bool
start_serial_device_analysis(const char *device_path, long baud_rate)
{
	if (setup_serial_device(device_path, baud_rate) == false) {
		fprintf(stderr, "Failed to setup %s\n", device_path);
		return false;
	}
	device_stream = fopen(device_path, "r");
	if (device_stream == NULL) {
		fprintf(stderr, "Failed to read %s\n", device_path);
		return false;
	}
	return true;
}

const char *
print_overview_menu_entry(size_t index)
{
	if ((index == 0) || (index == 2)) {
		return "+------+--------+--------+--------+--------+--------+--------+---------+---------+";
	} else if (index == 1) {
		return "| Addr |  Reads | Writes | MinLen | MaxLen | AvgLen | Unique |   T, ms |   f, Hz |";
	} else if (index - 3 < nodes_count) {
		index -= 3;
		sprintf(entry_content_buffer,
			"| %4d | %6zu | %6zu | %6zu | %6zu | %6.0lf | %6zu | %7.1Lf | %7.1Lf |",
			nodes[index].address,
			nodes[index].reads_count,
			nodes[index].writes_count,
			nodes[index].min_packet_size,
			nodes[index].max_packet_size,
			nodes[index].avg_packet_size,
			nodes[index].samples_count,
			nodes[index].period_ms,
			nodes[index].frequency_hz
		);
		return entry_content_buffer;
	}
	return "";
}

const char *
print_samples_menu_entry(size_t index)
{
	if ((index == 0) || (index == 2)) {
		return "+----------+---------+---------+---------+";
	} else if (index == 1) {
		return "| Quantity |   T, ms |   f, Hz | Content |";
	} else if (index - 3 < selected_node->samples_count) {
		index -= 3;
		sprintf(entry_content_buffer,
			"| %8zu | %7.1Lf | %7.1Lf | %s |",
			selected_node->samples[index].packets_count,
			selected_node->samples[index].period_ms,
			selected_node->samples[index].frequency_hz,
			selected_node->samples[index].packet_string
		);
		return entry_content_buffer;
	}
	return "";
}

void
stop_serial_device_analysis(void)
{
	device_handler_must_finish = true;
	pthread_join(device_handler_thread, NULL);
	fclose(device_stream);

	for (size_t i = 0; i < nodes_count; ++i) {
		for (size_t j = 0; j < nodes[i].samples_count; ++j) {
			free_i2c_packet(nodes[i].samples[j].packet);
			free(nodes[i].samples[j].packet_string);
		}
		free(nodes[i].samples);
	}
	free(nodes);
}

static void
enter_samples_menu(size_t node_index)
{
	selected_node = nodes + node_index;
	const size_t *view_sel = enter_list_menu(SAMPLES_MENU, selected_node->packets_count + 3);
	in_samples_menu = true;
	while (true) {
		pthread_mutex_lock(&interface_lock);
		int c = getch();
		pthread_mutex_unlock(&interface_lock);
		if (handle_list_menu_navigation(c) == true) {
			// Rest a little.
		} else if ((c == '\n') || (c == KEY_RIGHT)) {
			if (*view_sel > 2) {
				pthread_mutex_lock(&interface_lock);
				clear();
				refresh();
				mvprintw(0, 0, "%s", selected_node->samples[*view_sel - 3].packet_string);
				while (getch() == ERR) {
					nanosleep(&input_delay, NULL);
				}
				redraw_list_menu_unprotected();
				pthread_mutex_unlock(&interface_lock);
			}
		} else if ((c == 'q') || (c == KEY_LEFT)) {
			break;
		} else if (c == KEY_RESIZE) {
			resize_counter_action();
		}
		nanosleep(&input_delay, NULL);
	}
	in_samples_menu = false;
	leave_list_menu();
}

void
enter_overview_menu(void)
{
	const size_t *view_sel = enter_list_menu(OVERVIEW_MENU, 3);
	pthread_create(&device_handler_thread, NULL, &device_handler, NULL);
	while (true) {
		pthread_mutex_lock(&interface_lock);
		int c = getch();
		pthread_mutex_unlock(&interface_lock);
		if (handle_list_menu_navigation(c) == true) {
			// Rest a little.
		} else if ((c == '\n') || (c == KEY_RIGHT)) {
			if (*view_sel > 2) {
				enter_samples_menu(*view_sel - 3);
			}
		} else if (c == 'q') {
			break;
		} else if (c == KEY_RESIZE) {
			resize_counter_action();
		}
		nanosleep(&input_delay, NULL);
	}
}
