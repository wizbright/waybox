#ifndef CURSOR_H
#define CURSOR_H
#include <wayland-server.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

struct wb_cursor {
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
};

struct wb_cursor *wb_cursor_create();
void wb_cursor_destroy(struct wb_cursor *cursor);

#endif // cursor.h
