#include <stdlib.h>

#include "waybox/seat.h"

struct wb_seat * wb_seat_create(struct wb_server * server) {
	struct wb_seat * seat = malloc(sizeof(struct wb_seat));
	seat->seat = wlr_seat_create(server->wl_display, "seat0");
	wlr_seat_set_capabilities(seat->seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	return seat;
}

void wb_seat_destroy(struct wb_seat * seat) {
	wlr_seat_destroy(seat->seat);
	free(seat);
}
