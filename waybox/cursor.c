#include <stdlib.h>
#include "waybox/cursor.h"
#include "waybox/xdg_shell.h"

static void process_cursor_move(struct wb_server *server, uint32_t time) {
	/* Move the grabbed view to the new position. */
	server->grabbed_view->x = server->cursor->cursor->x - server->grab_x;
	server->grabbed_view->y = server->cursor->cursor->y - server->grab_y;
}

static void process_cursor_resize(struct wb_server *server, uint32_t time) {
	/*
	 * Resizing the grabbed view can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the view
	 * on one or two axes, but can also move the view if you resize from the top
	 * or left edges (or top-left corner).
	 *
	 * Note that I took some shortcuts here. In a more fleshed-out compositor,
	 * you'd wait for the client to prepare a buffer at the new size, then
	 * commit any movement that was prepared.
	 */
	struct wb_view *view = server->grabbed_view;
	double dx = server->cursor->cursor->x - server->grab_x;
	double dy = server->cursor->cursor->y - server->grab_y;
	double x = view->x;
	double y = view->y;
	int width = server->grab_width;
	int height = server->grab_height;
	if (server->resize_edges & WLR_EDGE_TOP) {
		y = server->grab_y + dy;
		height -= dy;
		if (height < 1) {
			y += height;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		height += dy;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		x = server->grab_x + dx;
		width -= dx;
		if (width < 1) {
			x += width;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		width += dx;
	}
	view->x = x;
	view->y = y;
	wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
}

static void process_cursor_motion(struct wb_server *server, uint32_t time) {
	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == WB_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == WB_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find the view under the pointer and send the event along. */
	double sx, sy;
	struct wlr_seat *seat = server->seat->seat;
	struct wlr_surface *surface = NULL;
	struct wb_view *view = desktop_view_at(server,
			server->cursor->cursor->x, server->cursor->cursor->y, &surface, &sx, &sy);
	if (!view) {
		/* If there's no view under the cursor, set the cursor image to a
		 * default. This is what makes the cursor image appear when you move it
		 * around the screen, not over any views. */
		wlr_xcursor_manager_set_cursor_image(
				server->cursor->xcursor_manager, "left_ptr", server->cursor->cursor);
	}
	if (surface) {
		bool focus_changed = seat->pointer_state.focused_surface != surface;
		/*
		 * "Enter" the surface if necessary. This lets the client know that the
		 * cursor has entered one of its surfaces.
		 *
		 * Note that this gives the surface "pointer focus", which is distinct
		 * from keyboard focus. You get pointer focus by moving the pointer over
		 * a window.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (!focus_changed) {
			/* The enter event contains coordinates, so we only need to notify
			 * on motion if the focus did not change. */
			wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct wb_cursor *cursor = wl_container_of(listener, cursor, cursor_motion);
	struct wlr_event_pointer_motion *event = data;
	wlr_cursor_move(cursor->cursor, event->device, event->delta_x, event->delta_y);
	process_cursor_motion(cursor->server, event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct wb_cursor *cursor = wl_container_of(listener, cursor, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(cursor->cursor, event->device, event->x, event->y);
	process_cursor_motion(cursor->server, event->time_msec);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct wb_cursor *cursor =
		wl_container_of(listener, cursor, cursor_button);
	struct wlr_event_pointer_button *event = data;
	/* Notify the client with pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(cursor->server->seat->seat,
			event->time_msec, event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface;
	struct wb_view *view = desktop_view_at(cursor->server,
			cursor->server->cursor->cursor->x, cursor->server->cursor->cursor->y, &surface, &sx, &sy);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* If you released any buttons, we exit interactive move/resize mode. */
		cursor->server->cursor_mode = WB_CURSOR_PASSTHROUGH;
	} else {
		/* Focus that client if the button was _pressed_ */
		focus_view(view, surface);
	}
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wb_cursor *cursor =
		wl_container_of(listener, cursor, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(cursor->server->seat->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	struct wb_cursor *cursor =
		wl_container_of(listener, cursor, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(cursor->server->seat->seat);
}

struct wb_cursor *wb_cursor_create() {
	struct wb_cursor *cursor = malloc(sizeof(struct wb_cursor));
	cursor->cursor = wlr_cursor_create();
	cursor->xcursor_manager = wlr_xcursor_manager_create("default", 24);

	cursor->cursor_motion.notify = handle_cursor_motion;
	wl_signal_add(&cursor->cursor->events.motion, &cursor->cursor_motion);

	cursor->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
	wl_signal_add(&cursor->cursor->events.motion_absolute, &cursor->cursor_motion_absolute);

	cursor->cursor_button.notify = handle_cursor_button;
	wl_signal_add(&cursor->cursor->events.button, &cursor->cursor_button);

	cursor->cursor_axis.notify = handle_cursor_axis;
	wl_signal_add(&cursor->cursor->events.axis, &cursor->cursor_axis);

	cursor->cursor_frame.notify = handle_cursor_frame;
	wl_signal_add(&cursor->cursor->events.frame, &cursor->cursor_frame);

	return cursor;
}

void wb_cursor_destroy(struct wb_cursor *cursor) {
	if (!cursor) {
		return;
	}

	wlr_xcursor_manager_destroy(cursor->xcursor_manager);
	wlr_cursor_destroy(cursor->cursor);
	free(cursor);
}
