#include <stdlib.h>
#include "waybox/cursor.h"

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct wb_cursor *cursor = wl_container_of(listener, cursor, cursor_motion);
	struct wlr_event_pointer_motion *event = data;
	wlr_cursor_move(cursor->cursor, event->device, event->delta_x, event->delta_y);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct wb_cursor *cursor = wl_container_of(listener, cursor, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(cursor->cursor, event->device, event->x, event->y);
}

struct wb_cursor *wb_cursor_create() {
	struct wb_cursor *cursor = malloc(sizeof(struct wb_cursor));
	cursor->cursor = wlr_cursor_create();
	cursor->xcursor_manager = wlr_xcursor_manager_create("default", 24);

	cursor->cursor_motion.notify = handle_cursor_motion;
	wl_signal_add(&cursor->cursor->events.motion, &cursor->cursor_motion);

	cursor->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
	wl_signal_add(&cursor->cursor->events.motion_absolute, &cursor->cursor_motion_absolute);

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
