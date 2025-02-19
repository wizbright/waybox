#include <wlr/types/wlr_idle_inhibit_v1.h>

#include "idle.h"

static void idle_inhibit_manager_destroy(struct wl_listener *listener, void *data) {
	struct wb_server *server = wl_container_of(listener, server, destroy_inhibit_manager);
	wl_list_remove(&server->new_inhibitor.link);
	wl_list_remove(&server->inhibitors);
}

static void idle_inhibitor_destroy(struct wl_listener *listener, void *data) {
	struct wb_server *server = wl_container_of(listener, server, destroy_inhibitor);
	/* wlroots will destroy the inhibitor after this callback, so this number will be 1 if the
	 * last inhibitor is being destroyed. */
	wl_list_remove(&server->destroy_inhibitor.link);
	wlr_idle_notifier_v1_set_inhibited(server->idle_notifier,
			wl_list_length(&server->inhibitors) > 1);
}

static void idle_inhibitor_new(struct wl_listener *listener, void *data) {
	struct wb_server *server = wl_container_of(listener, server, new_inhibitor);
	struct wlr_idle_inhibitor_v1 *inhibitor = data;

	server->destroy_inhibitor.notify = idle_inhibitor_destroy;
	wl_signal_add(&inhibitor->events.destroy, &server->destroy_inhibitor);
	wl_list_remove(&inhibitor->link);
	wl_list_insert(&server->inhibitors, &inhibitor->link);
	wlr_idle_notifier_v1_set_inhibited(server->idle_notifier, true);
}

bool create_idle_manager(struct wb_server *server) {
	server->idle_notifier = wlr_idle_notifier_v1_create(server->wl_display);
	server->idle_inhibit_manager = wlr_idle_inhibit_v1_create(server->wl_display);

	wl_list_init(&server->inhibitors);
	server->new_inhibitor.notify = idle_inhibitor_new;
	wl_signal_add(&server->idle_inhibit_manager->events.new_inhibitor, &server->new_inhibitor);
	server->destroy_inhibit_manager.notify = idle_inhibit_manager_destroy;
	wl_signal_add(&server->idle_inhibit_manager->events.destroy, &server->destroy_inhibit_manager);
	return true;
}
