#include <wlr/types/wlr_idle_inhibit_v1.h>

#include "idle.h"

void idle_inhibitor_new(struct wl_listener *listener, void *data) {
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, new_inhibitor);
	toplevel->inhibited = true;
}

void idle_inhibitor_destroy(struct wl_listener *listener, void *data) {
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, new_inhibitor);
	toplevel->inhibited = false;
}

bool create_idle_manager(struct wb_server *server) {
	server->idle_notifier = wlr_idle_notifier_v1_create(server->wl_display);
	server->idle_inhibitor = wlr_idle_inhibit_v1_create(server->wl_display);
	return true;
}

bool install_inhibitor(struct wb_toplevel *toplevel) {

	toplevel->new_inhibitor.notify = idle_inhibitor_new;
	wl_signal_add(&toplevel->server->idle_inhibitor->events.new_inhibitor, &toplevel->new_inhibitor);
	toplevel->destroy_inhibitor.notify = idle_inhibitor_destroy;
	wl_signal_add(&toplevel->server->idle_inhibitor->events.destroy, &toplevel->destroy_inhibitor);
	return true;
}
