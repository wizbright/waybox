#ifndef _WB_SEAT_H
#define _WB_SEAT_H

#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>

#include "waybox/server.h"

struct wb_seat {
	struct wlr_seat * seat;

	struct wl_list keyboards;
};

struct wb_keyboard {
	struct wl_list link;
	struct wb_server * server;
	struct wlr_input_device * device;

	struct wl_listener modifiers;
	struct wl_listener key;
};

struct wb_server;
struct wb_seat * wb_seat_create(struct wb_server * server);
void wb_seat_destroy(struct wb_seat * seat);
#endif
