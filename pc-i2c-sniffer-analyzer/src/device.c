#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <curses.h>

#define PACKET_MAX_SIZE 5000

struct node_data {
	int address;
	struct i2c_packet **packets;
	size_t packets_count;
	size_t reads_count;
	size_t writes_count;
	size_t min_packet_size;
	size_t max_packet_size;
	double avg_packet_size;
	long double period_ms;
	long double frequency_hz;
};

static const char *serial_file_paths[] = {"/dev/ttyUSB0", "/dev/ttyACM0"};
static const size_t serial_file_paths_count = 2;
static FILE *device_stream = NULL;
static pthread_t device_handler_thread;
static volatile bool device_handler_must_finish = false;
static struct node_data *nodes = NULL;
static size_t nodes_count = 0;

static const char *
get_path_to_available_serial_device(void)
{
	for (size_t i = 0; i < serial_file_paths_count; ++i) {
		if (access(serial_file_paths[i], F_OK) == 0) {
			return serial_file_paths[i];
		}
	}
	return NULL;
}

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

static FILE *
open_serial_device(const char *desired_path, long baud_rate)
{
	const char *device_path;
	if (desired_path == NULL) {
		device_path = get_path_to_available_serial_device();
	} else {
		device_path = desired_path;
	}
	if (device_path == NULL) {
		fprintf(stderr, "Failed to find serial device\n");
		return NULL;
	}
	if (setup_serial_device(device_path, baud_rate) == false) {
		fprintf(stderr, "Failed to setup %s\n", device_path);
		return NULL;
	}
	FILE *f = fopen(device_path, "r");
	if (f == NULL) {
		fprintf(stderr, "Failed to read %s\n", device_path);
	}
	return f;
}

static size_t
make_sure_node_exists(int address)
{
	for (size_t i = 0; i < nodes_count; ++i) {
		if (address == nodes[i].address) {
			return i;
		}
	}
	nodes = realloc(nodes, sizeof(struct node_data) * (nodes_count + 1));
	nodes[nodes_count].address = address;
	nodes[nodes_count].packets = NULL;
	nodes[nodes_count].packets_count = 0;
	nodes[nodes_count].reads_count = 0;
	nodes[nodes_count].writes_count = 0;
	nodes[nodes_count].min_packet_size = 999999;
	nodes[nodes_count].max_packet_size = 0;
	nodes[nodes_count].avg_packet_size = 0;
	nodes[nodes_count].period_ms = 0;
	nodes[nodes_count].frequency_hz = 0;
	nodes_count += 1;
	return nodes_count - 1;
}

static void
assign_packet_to_node(uint8_t index, struct i2c_packet *packet)
{
	nodes[index].packets = realloc(nodes[index].packets, sizeof(struct i2c_packets *) * (nodes[index].packets_count + 1));
	nodes[index].packets[nodes[index].packets_count] = packet;
	if (packet->is_read == true) {
		nodes[index].reads_count += 1;
	} else {
		nodes[index].writes_count += 1;
	}
	if (packet->data_len < nodes[index].min_packet_size) {
		nodes[index].min_packet_size = packet->data_len;
	}
	if (packet->data_len > nodes[index].max_packet_size) {
		nodes[index].max_packet_size = packet->data_len;
	}
	nodes[index].avg_packet_size = nodes[index].avg_packet_size * nodes[index].packets_count + packet->data_len;
	nodes[index].packets_count += 1;
	nodes[index].avg_packet_size /= nodes[index].packets_count;
	nodes[index].period_ms = ((long double)packet->gettime.tv_sec - nodes[index].packets[0]->gettime.tv_sec + (packet->gettime.tv_nsec - nodes[index].packets[0]->gettime.tv_nsec) / 1000000000.0L) / nodes[index].packets_count * 1000.0L;
	nodes[index].frequency_hz = 1000.0L / nodes[index].period_ms;
}

static void *
device_handler(void *dummy)
{
	(void)dummy;
	char c;
	char packet[PACKET_MAX_SIZE];
	size_t packet_len = 0;
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
			node_index = make_sure_node_exists(parsed_packet->data[0]);
			assign_packet_to_node(node_index, parsed_packet);
			packet_len = 0;
		} else if (c == EOF) {
			break;
		} else if (packet_len < PACKET_MAX_SIZE) {
			packet[packet_len++] = c;
		}
	}
	return NULL;
}

bool
start_serial_device_analysis(const char *desired_path, long baud_rate)
{
	device_stream = open_serial_device(desired_path, baud_rate);
	if (device_stream == NULL) {
		return false;
	}
	pthread_create(&device_handler_thread, NULL, &device_handler, NULL);
	return true;
}

static void
print_overall_statistics(void)
{
	mvprintw(0, 0, "space:update enter:submit q:quit");
	mvprintw(1, 0, "+----+------+--------+--------+--------+--------+--------+--------+--------+");
	mvprintw(2, 0, "| ID | Addr |  Reads | Writes | MinLen | MaxLen | AvgLen |  T, ms |  f, Hz |");
	mvprintw(3, 0, "+----+------+--------+--------+--------+--------+--------+--------+--------+");
	for (size_t i = 0; i < nodes_count; ++i) {
		mvprintw(4 + i, 0, "| %2zu | %4d | %6zu | %6zu | %6zu | %6zu | %6.0lf | %6.1Lf | %6.1Lf |",
			i + 1,
			nodes[i].address,
			nodes[i].reads_count,
			nodes[i].writes_count,
			nodes[i].min_packet_size,
			nodes[i].max_packet_size,
			nodes[i].avg_packet_size,
			nodes[i].period_ms,
			nodes[i].frequency_hz);
	}
	mvprintw(4 + nodes_count, 0, "+----+------+--------+--------+--------+--------+--------+--------+--------+");
}

static void
print_channel_statistics(size_t i)
{
	if ((i >= nodes_count) || (nodes[i].packets_count == 0)) {
		return;
	}
	mvprintw(0, 0, "%s", "");
	int terminal_height = LINES;
	for (size_t j = nodes[i].packets_count; (j > 0) && (terminal_height > 0); --j) {
		printw("%6zu.", j);
		printw(nodes[i].packets[j - 1]->is_read ? " R" : " W");
		for (size_t k = 0; k < nodes[i].packets[j - 1]->data_len; ++k) {
			printw(" %3d", nodes[i].packets[j - 1]->data[k]);
		}
		printw("\n\n");
		terminal_height -= 2;
	}
}

void
print_analysis_results(size_t selected_channel)
{
	clear();
	if (selected_channel == 0) {
		print_overall_statistics();
	} else {
		print_channel_statistics(selected_channel - 1);
	}
	refresh();
}

void
stop_serial_device_analysis(void)
{
	device_handler_must_finish = true;
	pthread_join(device_handler_thread, NULL);
	fclose(device_stream);
}
