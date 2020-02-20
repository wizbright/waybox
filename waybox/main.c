//#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/backend.h>

#include "waybox/server.h"

//struct wl_display* display = NULL;

int main(int argc, char **argv){
	char *startup_cmd;
	if (argc > 0) {
		int i;
		for (i = 0; i < argc; i++)
		{
			if (!strcmp("--debug", argv[i]) || !strcmp("-v", argv[i]) || !strcmp("--exit", argv[i])) {
				printf("Warning: option %s is currently unimplemented\n", argv[i]);
			}
			else if ((!strcmp("--startup", argv[i]) || !strcmp("-s", argv[i])) && i < argc) {
				startup_cmd = argv[i + 1];
			}
			else if (!strcmp("--version", argv[i]) || !strcmp("-V", argv[i])) {
				printf(VERSION "\n");
				return 0;
			}
			else if (argv[i][0] == '-') {
				printf("Usage: %s [--debug] [--exit] [--help] [--startup CMD] [--version]\n", argv[0]);
				return strcmp("--help", argv[i]) != 0 && strcmp("-h", argv[i]) != 0;
			}
		}
	}

	struct wb_server server = {0};

	// Global display
	//display = server.wl_display;

	if (init_wb(&server) == false) {
		printf("Failed to create backend\n");
		exit(EXIT_FAILURE);
	}

	if (!start_wb(&server)) {
		printf("Failed to start server\n");
		terminate_wb(&server);
		exit(EXIT_FAILURE);
	}

	if (startup_cmd) {
		if (fork() == 0){
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
		}
	}

	wl_display_run(server.wl_display);

	terminate_wb(&server);

	return 0;
}
