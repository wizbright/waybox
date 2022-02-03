#include <unistd.h>

#include "waybox/seat.h"
#include "waybox/xdg_shell.h"

static bool cycle_views(struct wb_server *server) {
	/* Cycle to the next view */
	if (wl_list_length(&server->views) < 2) {
		return false;
	}
	struct wb_view *current_view = wl_container_of(
		server->views.prev, current_view, link);
	struct wb_view *prev_view = wl_container_of(
		server->views.next, prev_view, link);
	focus_view(current_view, current_view->xdg_surface->surface);
	/* Move the current view to the beginning of the list */
	wl_list_remove(&current_view->link);
	wl_list_insert(&server->views, &current_view->link);
	return true;
}

static bool cycle_views_reverse(struct wb_server *server) {
	/* Cycle to the previous view */
	if (wl_list_length(&server->views) < 2) {
		return false;
	}
	struct wb_view *current_view = wl_container_of(
		server->views.next, current_view, link);
	struct wb_view *next_view = wl_container_of(
		current_view->link.next, next_view, link);
	focus_view(next_view, next_view->xdg_surface->surface);
	/* Move the previous view to the end of the list */
	wl_list_remove(&current_view->link);
	wl_list_insert(server->views.prev, &current_view->link);
	return true;
}

static bool handle_keybinding(struct wb_server *server, xkb_keysym_t sym, uint32_t modifiers) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * Returns true if the keybinding is handled, false to send it to the
	 * client.
	 */

	struct wb_key_binding *key_binding;
	if (!server->config)
	{
		if (modifiers & WLR_MODIFIER_ALT && sym == XKB_KEY_Tab)
			cycle_views(server);
		else if (modifiers & (WLR_MODIFIER_ALT|WLR_MODIFIER_SHIFT) &&
				sym == XKB_KEY_Tab)
			cycle_views_reverse(server);
		else if (sym == XKB_KEY_Escape && modifiers & WLR_MODIFIER_CTRL)
					wl_display_terminate(server->wl_display);
		else
			return false;
		return true;
	}
	wl_list_for_each(key_binding, &server->config->key_bindings, link) {
		if (sym == key_binding->sym && modifiers == key_binding->modifiers)
		{
			if ((strcmp("NextWindow", key_binding->action) == 0)) {
				return cycle_views(server);
			}
			else if ((strcmp("PreviousWindow", key_binding->action) == 0)) {
				return cycle_views_reverse(server);
			}
			else if ((strcmp("Close", key_binding->action) == 0)) {
				struct wb_view *current_view = wl_container_of(
						server->views.next, current_view, link);
				wlr_xdg_toplevel_send_close(current_view->xdg_surface);
				return true;
			}
			else if ((strcmp("Execute", key_binding->action) == 0)) {
				if (fork() == 0) {
					execl("/bin/sh", "/bin/sh", "-c", key_binding->cmd, (char *) NULL);
				}
				return true;
			}
			else if ((strcmp("Exit", key_binding->action) == 0)) {
					wl_display_terminate(server->wl_display);
					return true;
			}
		}
	}
	return false;
}

static void keyboard_handle_modifiers(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct wb_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(keyboard->server->seat->seat, keyboard->device);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat->seat,
		&keyboard->device->keyboard->modifiers);
}

static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct wb_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wb_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->device->keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i], modifiers);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void handle_new_keyboard(struct wb_server *server,
		struct wlr_input_device *device) {
	struct wb_keyboard *keyboard =
		calloc(1, sizeof(struct wb_keyboard));
	keyboard->server = server;
	keyboard->device = device;

	/* We need to prepare an XKB keymap and assign it to the keyboard. */
	struct xkb_rule_names rules = {0};
	if (server->config && server->config->keyboard_layout.layout)
		rules.layout = server->config->keyboard_layout.layout;
	else
		rules.layout = getenv("XKB_DEFAULT_LAYOUT");
	if (server->config && server->config->keyboard_layout.model)
		rules.model = server->config->keyboard_layout.model;
	else
		rules.model = getenv("XKB_DEFAULT_MODEL");
	if (server->config && server->config->keyboard_layout.options)
		rules.options = server->config->keyboard_layout.options;
	else
		rules.options = getenv("XKB_DEFAULT_OPTIONS");
	if (server->config && server->config->keyboard_layout.rules)
		rules.rules = server->config->keyboard_layout.rules;
	else
		rules.rules = getenv("XKB_DEFAULT_RULES");
	if (server->config && server->config->keyboard_layout.variant)
		rules.variant = server->config->keyboard_layout.variant;
	else
		rules.variant = getenv("XKB_DEFAULT_VARIANT");

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	if (keymap != NULL) {
		wlr_keyboard_set_keymap(device->keyboard, keymap);
		wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);
	}
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(server->seat->seat, device);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->seat->keyboards, &keyboard->link);
}

static void new_input_notify(struct wl_listener *listener, void *data) {
	struct wlr_input_device *device = data;
	struct wb_server *server = wl_container_of(listener, server, new_input);
	switch (device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			wlr_log(WLR_INFO, "%s: %s", _("New keyboard detected"), device->name);
			handle_new_keyboard(server, device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			wlr_log(WLR_INFO, "%s: %s", _("New pointer detected"), device->name);
			wlr_cursor_attach_input_device(server->cursor->cursor, device);
			break;
		default:
			wlr_log(WLR_INFO, "%s: %s", _("Unsupported input device detected"), device->name);
			break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->seat->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat->seat, caps);
}

static void handle_request_set_primary_selection(struct wl_listener *listener,
		void *data) {
	struct wb_seat *seat =
		wl_container_of(listener, seat, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat->seat, event->source, event->serial);
}

static void handle_request_set_selection(struct wl_listener *listener, void
		*data) {
	struct wb_seat *seat =
		wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

struct wb_seat *wb_seat_create(struct wb_server *server) {
	struct wb_seat *seat = malloc(sizeof(struct wb_seat));

	wl_list_init(&seat->keyboards);
	server->new_input.notify = new_input_notify;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
	seat->seat = wlr_seat_create(server->wl_display, "seat0");

	wlr_primary_selection_v1_device_manager_create(server->wl_display);
	seat->request_set_primary_selection.notify =
		handle_request_set_primary_selection;
	wl_signal_add(&seat->seat->events.request_set_primary_selection,
			&seat->request_set_primary_selection);
	seat->request_set_selection.notify = handle_request_set_selection;
	wl_signal_add(&seat->seat->events.request_set_selection,
			&seat->request_set_selection);

	return seat;
}

void wb_seat_destroy(struct wb_seat *seat) {
	wl_list_remove(&seat->request_set_primary_selection.link);
	wl_list_remove(&seat->request_set_selection.link);
	wlr_seat_destroy(seat->seat);
	free(seat);
}
