#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/backend.h>

#include "waybox/server.h"

struct wl_display* display = NULL;

int main(int argc, char **argv){
	struct wb_server server = {0};

	// Global display
	display = server.wl_display;

	if (init_wb(&server) == false) {
		printf("Failed to create backend\n");
		exit(EXIT_FAILURE);
	}

	if (!start_wb(&server)) {
		printf("Failed to start server\n");
		terminate_wb(&server);
		exit(EXIT_FAILURE);
	}

	wl_display_run(server.wl_display);

	terminate_wb(&server);

	return 0;
}
