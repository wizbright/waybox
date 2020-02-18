#include "waybox/server.h"
#include "waybox/seat.h"

/* Stolen from wltiny.  Customizations will come later. */
static bool handle_keybinding(struct wb_server *server, xkb_keysym_t sym) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * This function assumes Alt is held down.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
#if 0
		/* Cycle to the next view */
		if (wl_list_length(&server->views) < 2) {
			break;
		}
		struct tinywl_view *current_view = wl_container_of(
			server->views.next, current_view, link);
		struct tinywl_view *next_view = wl_container_of(
			current_view->link.next, next_view, link);
		focus_view(next_view, next_view->xdg_surface->surface);
		/* Move the previous view to the end of the list */
		wl_list_remove(&current_view->link);
		wl_list_insert(server->views.prev, &current_view->link);
#endif
		break;
	default:
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
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WLR_KEY_PRESSED) {
		/* If alt is held down and this button was _pressed_, we attempt to
		 * process it as a compositor keybinding. */
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void server_new_keyboard(struct wb_server *server,
		struct wlr_input_device *device) {
	struct wb_keyboard *keyboard =
		calloc(1, sizeof(struct wb_keyboard));
	keyboard->server = server;
	keyboard->device = device;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_rule_names rules = { 0 };
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
			server_new_keyboard(server, device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			wlr_cursor_attach_input_device(server->cursor->cursor, device);
			break;
		default:
			break;
	}
}

bool init_wb(struct wb_server* server) {

    // create display
    server->wl_display = wl_display_create();
	assert(server->wl_display);

    // create shared memory
    wl_display_init_shm(server->wl_display);

    // event loop stuff
    server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
	assert(server->wl_event_loop);

    // create backend
	server->backend = wlr_backend_autocreate(server->wl_display, NULL);
	assert(server->backend);
    if(server->backend == NULL){
        printf("Failed to create backend\n");
        return false;
    }

	server->layout = wlr_output_layout_create();
	server->cursor = wb_cursor_create();
	wlr_cursor_attach_output_layout(server->cursor->cursor, server->layout);
	server->seat = wb_seat_create(server);

    return true;
}

bool start_wb(struct wb_server* server) {
    wl_list_init(&server->outputs);

	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	server->new_input.notify = new_input_notify;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);

	const char *socket = wl_display_add_socket_auto(server->wl_display);
	assert(socket);

    if (!wlr_backend_start(server->backend)) {
		fprintf(stderr, "Failed to start backend\n");
		wl_display_destroy(server->wl_display);
		return false;
	}

    //printf("Running Wayland compositor on Wayland display '%s'\n", socket);
    //setenv("WAYLAND_DISPLAY", socket, true);

    wlr_gamma_control_manager_v1_create(server->wl_display);
	wlr_screencopy_manager_v1_create(server->wl_display);
	wlr_gtk_primary_selection_device_manager_create(server->wl_display);
	wlr_idle_create(server->wl_display);

	server->compositor = wlr_compositor_create(server->wl_display,
			wlr_backend_get_renderer(server->backend));
	wlr_data_device_manager_create(server->wl_display);
	wlr_xdg_shell_create(server->wl_display);
    //wlr_idle_create(server->wl_display);

    return true;
}

bool terminate_wb(struct wb_server* server) {
    wl_display_destroy(server->wl_display);

    printf("Display destroyed.\n");

	wb_cursor_destroy(server->cursor);
	wb_seat_destroy(server->seat);
	wlr_output_layout_destroy(server->layout);

    return true;
}
