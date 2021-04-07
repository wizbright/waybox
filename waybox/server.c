#include "waybox/server.h"
#include "waybox/xdg_shell.h"

bool wb_create_backend(struct wb_server* server) {
	// create display
	server->wl_display = wl_display_create();
	if (server->wl_display == NULL) {
		wlr_log(WLR_ERROR, "%s", _("Failed to connect to a Wayland display"));
		return false;
	}

	// create backend
	server->backend = wlr_backend_autocreate(server->wl_display);
	if (server->backend == NULL) {
		return false;
	}

	server->renderer = wlr_backend_get_renderer(server->backend);
	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	server->compositor = wlr_compositor_create(server->wl_display,
			server->renderer);
	server->layout = wlr_output_layout_create();
	server->seat = wb_seat_create(server);
	server->cursor = wb_cursor_create(server);

	return true;
}

bool wb_start_server(struct wb_server* server) {
	wl_list_init(&server->outputs);

	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	const char *socket = wl_display_add_socket_auto(server->wl_display);
	if (!socket) {
		wlr_backend_destroy(server->backend);
		return false;
	}

	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "%s", _("Failed to start backend"));
		wlr_backend_destroy(server->backend);
		wl_display_destroy(server->wl_display);
		return false;
	}

	wlr_log(WLR_INFO, "%s: WAYLAND_DISPLAY=%s", _("Running Wayland compositor on Wayland display"), socket);
	setenv("WAYLAND_DISPLAY", socket, true);

	wlr_gamma_control_manager_v1_create(server->wl_display);
	wlr_screencopy_manager_v1_create(server->wl_display);
	wlr_idle_create(server->wl_display);

	wlr_data_device_manager_create(server->wl_display);
	wl_list_init(&server->views);
	init_xdg_shell(server);

	return true;
}

bool wb_terminate(struct wb_server* server) {
	wb_cursor_destroy(server->cursor);
	wb_seat_destroy(server->seat);
	wl_display_destroy_clients(server->wl_display);
	wl_display_destroy(server->wl_display);
	wlr_output_layout_destroy(server->layout);

	wlr_log(WLR_INFO, "%s", _("Display destroyed"));

	return true;
}
