#include <libevdev/libevdev.h>
#include <unistd.h>

#include <wlr/config.h>
#if WLR_HAS_LIBINPUT_BACKEND && defined(HAS_LIBINPUT)
#include <wlr/backend/libinput.h>
#else
#undef HAS_LIBINPUT
#endif
#include <wlr/backend/session.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>

#include "waybox/seat.h"
#include "waybox/xdg_shell.h"

static void deiconify_toplevel(struct wb_toplevel *toplevel) {
	if (toplevel->xdg_toplevel->requested.minimized) {
		toplevel->xdg_toplevel->requested.minimized = false;
		wl_signal_emit(&toplevel->xdg_toplevel->events.request_minimize, NULL);
	}
}

static void cycle_toplevels(struct wb_server *server) {
	/* Cycle to the next toplevel */
	if (wl_list_length(&server->toplevels) < 1) {
		return;
	}

	struct wb_toplevel *current_toplevel = wl_container_of(
		server->toplevels.prev, current_toplevel, link);
	deiconify_toplevel(current_toplevel);
	focus_toplevel(current_toplevel);

	/* Move the current toplevel to the beginning of the list */
	wl_list_remove(&current_toplevel->link);
	wl_list_insert(&server->toplevels, &current_toplevel->link);
}

static void cycle_toplevels_reverse(struct wb_server *server) {
	/* Cycle to the previous toplevel */
	if (wl_list_length(&server->toplevels) < 1) {
		return;
	}

	struct wb_toplevel *current_toplevel = wl_container_of(
		server->toplevels.next, current_toplevel, link);
	struct wb_toplevel *next_toplevel = wl_container_of(
		current_toplevel->link.next, next_toplevel, link);
	deiconify_toplevel(next_toplevel);
	focus_toplevel(next_toplevel);

	/* Move the current toplevel to after the previous toplevel in the list */
	wl_list_remove(&current_toplevel->link);
	wl_list_insert(server->toplevels.prev, &current_toplevel->link);
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

	/* TODO: Make these configurable through rc.xml */
	if (modifiers & (WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT) &&
		sym >= XKB_KEY_XF86Switch_VT_1 &&
		sym <= XKB_KEY_XF86Switch_VT_12) {
		unsigned int vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
		wlr_session_change_vt (server->session, vt);

		return true;
	}

	if (!server->config) {
		/* Some default key bindings, when the rc.xml file can't be
		 * parsed. */
		if (modifiers & WLR_MODIFIER_ALT && sym == XKB_KEY_Tab)
			cycle_toplevels(server);
		else if (modifiers & (WLR_MODIFIER_ALT|WLR_MODIFIER_SHIFT) &&
				sym == XKB_KEY_Tab)
			cycle_toplevels_reverse(server);
		else if (sym == XKB_KEY_Escape && modifiers & WLR_MODIFIER_CTRL)
			wl_display_terminate(server->wl_display);
		else
			return false;
		return true;
	}

	struct wb_key_binding *key_binding;
	wl_list_for_each(key_binding, &server->config->key_bindings, link) {
		if (sym == key_binding->sym && modifiers == key_binding->modifiers) {
			if (key_binding->action & ACTION_NEXT_WINDOW)
				cycle_toplevels(server);
			if (key_binding->action & ACTION_PREVIOUS_WINDOW)
				cycle_toplevels_reverse(server);
			if (key_binding->action & ACTION_CLOSE) {
				struct wb_toplevel *current_toplevel = wl_container_of(
						server->toplevels.next, current_toplevel, link);
				if (current_toplevel->scene_tree->node.enabled)
					wlr_xdg_toplevel_send_close(current_toplevel->xdg_toplevel);
			}
			if (key_binding->action & ACTION_EXECUTE) {
				if (fork() == 0) {
					execl("/bin/sh", "/bin/sh", "-c", key_binding->cmd, (char *) NULL);
				}
			}
			if (key_binding->action & ACTION_TOGGLE_MAXIMIZE) {
				struct wb_toplevel *toplevel = wl_container_of(server->toplevels.next, toplevel, link);
				if (toplevel->scene_tree->node.enabled)
					wl_signal_emit(&toplevel->xdg_toplevel->events.request_maximize, NULL);
			}
			if (key_binding->action & ACTION_ICONIFY) {
				struct wb_toplevel *toplevel = wl_container_of(server->toplevels.next, toplevel, link);
				if (toplevel->scene_tree->node.enabled) {
					toplevel->xdg_toplevel->requested.minimized = true;
					wl_signal_emit(&toplevel->xdg_toplevel->events.request_minimize, NULL);
				}
			}
			if (key_binding->action & ACTION_SHADE) {
				struct wb_toplevel *toplevel = wl_container_of(server->toplevels.next, toplevel, link);
				if (toplevel->scene_tree->node.enabled) {
#if WLR_CHECK_VERSION(0, 19, 0)
					struct wlr_box geo_box = toplevel->xdg_toplevel->base->geometry;
#else
					struct wlr_box geo_box;
					wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);
#endif
					int decoration_height = MAX(geo_box.y - toplevel->geometry.y, TITLEBAR_HEIGHT);

					toplevel->previous_geometry = toplevel->geometry;
					wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
							toplevel->geometry.width, decoration_height);
				}
			}
			if (key_binding->action & ACTION_UNSHADE) {
				struct wb_toplevel *toplevel = wl_container_of(server->toplevels.next, toplevel, link);
				if (toplevel->previous_geometry.height > 0 && toplevel->previous_geometry.width > 0 && toplevel->scene_tree->node.enabled) {
					wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
							toplevel->previous_geometry.width, toplevel->previous_geometry.height);
				}
			}
			if (key_binding->action & ACTION_RECONFIGURE) {
				deinit_config(server->config);
				init_config(server);
			}
			if (key_binding->action & ACTION_EXIT)
				wl_display_terminate(server->wl_display);
			return true;
		}
	}
	return false;
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	/* This event is raised by the keyboard base wlr_input_device to signal
	 * the destruction of the wlr_keyboard. It will no longer receive events
	 * and should be destroyed.
	 */
	struct wb_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
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
	wlr_seat_set_keyboard(keyboard->server->seat->seat, keyboard->keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat->seat,
		&keyboard->keyboard->modifiers);
}

static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct wb_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wb_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i], modifiers);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}

	wlr_idle_notifier_v1_notify_activity(server->idle_notifier, seat);
}

static void handle_new_keyboard(struct wb_server *server,
		struct wlr_input_device *device) {
	struct wb_keyboard *keyboard =
		calloc(1, sizeof(struct wb_keyboard));
	keyboard->server = server;
	keyboard->keyboard = wlr_keyboard_from_input_device(device);

	/* We need to prepare an XKB keymap and assign it to the keyboard. */
	struct xkb_rule_names *rules = malloc(sizeof(struct xkb_rule_names));
	if (server->config && server->config->keyboard_layout.use_config) {
		if (server->config->keyboard_layout.layout)
			rules->layout = server->config->keyboard_layout.layout;
		if (server->config->keyboard_layout.model)
			rules->model = server->config->keyboard_layout.model;
		if (server->config->keyboard_layout.options)
			rules->options = server->config->keyboard_layout.options;
		if (server->config->keyboard_layout.rules)
			rules->rules = server->config->keyboard_layout.rules;
		if (server->config->keyboard_layout.variant)
			rules->variant = server->config->keyboard_layout.variant;
	}
	else
		/* If a NULL xkb_rule_names pointer is passed to
		   xkb_keymap_new_from_names, libxkbcommon will default to reading
		   the XKB_* env variables. So there's no need to do it ourselves. */
		rules = NULL;

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	if (keymap != NULL) {
		wlr_keyboard_set_keymap(keyboard->keyboard, keymap);
		wlr_keyboard_set_repeat_info(keyboard->keyboard, 25, 600);
	}
	free(rules);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	/* Here we set up listeners for keyboard events. */
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&keyboard->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&keyboard->keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(server->seat->seat, keyboard->keyboard);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->seat->keyboards, &keyboard->link);
}

#ifdef HAS_LIBINPUT
static bool libinput_config_get_enabled(char *config) {
	return strcmp(config, "disabled") != 0;
}
#endif

static void handle_new_pointer(struct wb_server *server, struct wlr_input_device *device) {
#ifdef HAS_LIBINPUT
	struct wb_config *config = server->config;
	if (wlr_input_device_is_libinput(device) && config->libinput_config.use_config) {
		struct libinput_device *libinput_handle =
			wlr_libinput_get_device_handle(device);

		if (config->libinput_config.accel_profile) {
			enum libinput_config_accel_profile accel_profile =
				LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
			if (strcmp(config->libinput_config.accel_profile, "flat") == 0)
				accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
			else if (strcmp(config->libinput_config.accel_profile, "none") == 0)
				accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
			libinput_device_config_accel_set_profile(libinput_handle, accel_profile);
		}
		if (config->libinput_config.accel_speed) {
			double accel_speed = strtod(config->libinput_config.accel_speed, NULL);
			libinput_device_config_accel_set_speed(libinput_handle, accel_speed);
		}
		if (config->libinput_config.calibration_matrix) {
			float matrix[6];
			unsigned short i = 0;
			while ((matrix[i] = strtod(strtok(config->libinput_config.calibration_matrix, " "), NULL) && i < 6)) {
				config->libinput_config.calibration_matrix = NULL;
				i++;
			}
			libinput_device_config_calibration_set_matrix(libinput_handle, matrix);
		}
		if (config->libinput_config.click_method) {
			enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
			if (strcmp(config->libinput_config.click_method, "clickfinger") == 0)
				click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
			else if (strcmp(config->libinput_config.click_method, "none") == 0)
				click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
			libinput_device_config_click_set_method(libinput_handle, click_method);
		}
		if (config->libinput_config.dwt) {
			libinput_device_config_dwt_set_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.dwt));
		}
		if (config->libinput_config.dwtp) {
			libinput_device_config_dwtp_set_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.dwtp));
		}
		if (config->libinput_config.left_handed) {
			libinput_device_config_left_handed_set(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.left_handed));
		}
		if (config->libinput_config.middle_emulation) {
			libinput_device_config_middle_emulation_set_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.middle_emulation));
		}
		if (config->libinput_config.natural_scroll) {
			libinput_device_config_scroll_set_natural_scroll_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.natural_scroll));
		}
		if (config->libinput_config.scroll_button) {
			int button = libevdev_event_code_from_name(EV_KEY, config->libinput_config.scroll_button);
			if (button != -1) {
				libinput_device_config_scroll_set_button(libinput_handle, button);
			}
		}
		if (config->libinput_config.scroll_button_lock) {
			libinput_device_config_scroll_set_button_lock(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.scroll_button_lock));
		}
		if (config->libinput_config.scroll_method) {
			enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
			if (strcmp(config->libinput_config.scroll_method, "edge") == 0)
				scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
			else if (strcmp(config->libinput_config.scroll_method, "none") == 0)
				scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
			else if (strcmp(config->libinput_config.scroll_method, "button") == 0)
				scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
			libinput_device_config_scroll_set_method(libinput_handle, scroll_method);
		}
		if (config->libinput_config.tap) {
			libinput_device_config_tap_set_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.tap));
		}
		if (config->libinput_config.tap_button_map) {
			enum libinput_config_tap_button_map map = LIBINPUT_CONFIG_TAP_MAP_LRM;
			if (strcmp(config->libinput_config.tap_button_map, "lmr") == 0)
				map = LIBINPUT_CONFIG_TAP_MAP_LMR;
			libinput_device_config_tap_set_button_map(libinput_handle, map);
		}
		if (config->libinput_config.tap_drag) {
			libinput_device_config_tap_set_drag_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.tap_drag));
		};
		if (config->libinput_config.tap_drag_lock) {
			libinput_device_config_tap_set_drag_lock_enabled(libinput_handle,
					libinput_config_get_enabled(config->libinput_config.tap_drag_lock));
		};
	}
#endif

	wlr_cursor_attach_input_device(server->cursor->cursor, device);
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
			handle_new_pointer(server, device);
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

void seat_focus_surface(struct wb_seat *seat, struct wlr_surface *surface) {
	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(seat->seat);
		return;
	}

	struct wlr_keyboard *kb = wlr_seat_get_keyboard(seat->seat);
	if (kb != NULL) {
		wlr_seat_keyboard_notify_enter(seat->seat, surface, kb->keycodes,
			kb->num_keycodes, &kb->modifiers);
	}
}

void seat_set_focus_layer(struct wb_seat *seat, struct wlr_layer_surface_v1 *layer) {
	if (!layer) {
		seat->focused_layer = NULL;
		return;
	}
	seat_focus_surface(seat, layer->surface);
	if (layer->current.layer > ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
		seat->focused_layer = layer;
	}
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
	wl_list_remove(&seat->keyboards);
	wl_list_remove(&seat->request_set_primary_selection.link);
	wl_list_remove(&seat->request_set_selection.link);
	free(seat);
}
