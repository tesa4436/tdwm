#include <xcb/xcb.h>
typedef struct win {
	xcb_window_t window;
	uint32_t x, y, width, height;
	uint32_t prev_x, prev_y, prev_width, prev_height;
	uint8_t flags;
	struct win *east_next;
	struct win *south_next;
	struct win *north_prev;
	struct win *west_prev;
} Window;

