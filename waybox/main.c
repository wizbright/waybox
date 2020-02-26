#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>

#include "waybox/server.h"

int main(int argc, char **argv) {
	char *startup_cmd = NULL;
	if (argc > 0) {
		int i;
		for (i = 0; i < argc; i++) {
			if (!strcmp("--debug", argv[i]) || !strcmp("-v", argv[i]) || !strcmp("--exit", argv[i])) {
				printf("Warning: option %s is currently unimplemented\n", argv[i]);
			} else if ((!strcmp("--startup", argv[i]) || !strcmp("-s", argv[i])) && i < argc) {
				startup_cmd = argv[i + 1];
			} else if (!strcmp("--version", argv[i]) || !strcmp("-V", argv[i])) {
				printf(VERSION "\n");
				return 0;
			} else if (argv[i][0] == '-') {
				printf("Usage: %s [--debug] [--exit] [--help] [--startup CMD] [--version]\n", argv[0]);
				return strcmp("--help", argv[i]) != 0 && strcmp("-h", argv[i]) != 0;
			}
		}
	}

	struct wb_server server = {0};

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
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
		}
	}

	wl_display_run(server.wl_display);

	terminate_wb(&server);

	return 0;
}
