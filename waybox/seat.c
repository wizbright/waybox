#include <unistd.h>

#include "waybox/seat.h"
#include "waybox/xdg_shell.h"

static bool handle_keybinding(struct wb_server *server, xkb_keysym_t sym, uint32_t modifiers) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */

	if (modifiers & WLR_MODIFIER_CTRL && sym == XKB_KEY_Escape) {
		wl_display_terminate(server->wl_display);
	} else if (modifiers & WLR_MODIFIER_ALT && sym == XKB_KEY_Tab) {
		/* Cycle to the next view */
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
	} else if (modifiers & WLR_MODIFIER_ALT && sym == XKB_KEY_F2) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", "(obrun || bemenu-run || synapse || gmrun || gnome-do || dmenu_run) 2>/dev/null", (char *) NULL);
		}
	} else {
		return false;
	}
	return true;
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
	if (event->state == WLR_KEY_PRESSED) {
		/* If alt is held down and this button was _pressed_, we attempt to
		 * process it as a compositor keybinding. */
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
	struct xkb_rule_names rules = {
		.rules = getenv("XKB_DEFAULT_RULES"),
		.model = getenv("XKB_DEFAULT_MODEL"),
		.layout = getenv("XKB_DEFAULT_LAYOUT"),
		.variant = getenv("XKB_DEFAULT_VARIANT"),
		.options = getenv("XKB_DEFAULT_OPTIONS"),
	};
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(device->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

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
			handle_new_keyboard(server, device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			wlr_cursor_attach_input_device(server->cursor->cursor, device);
			break;
		default:
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
