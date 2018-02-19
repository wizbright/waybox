#include <stdio.h>
#include <wayland-server.h>

struct wb_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
};

int main(int argc, char **argv){
	struct wb_server server;

	server.wl_display = wl_display_create();
	assert(server.wl_display);
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
	assert(server.wl_event_loop);

	server.backend = wlr_backend_autocreate(server.wl_display);
	assert(server.backend);

	if (!wlr_backend_start(server.backend)) {
		fprintf(stderr, "Failed to start backend\n");
		wl_display_destroy(server.wl_display);
		return 1;
	}

	wl_display_run(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
