#include <xcb/xcb.h>

enum { EAST, SOUTH, NORTH, WEST};
enum { X, Y, WIDTH, HEIGHT, GROUPWIDTH, GROUPHEIGHT};

typedef struct win {
	xcb_window_t wnd;
	uint32_t dim[6];
	struct win *n[4];
} Window;

struct stack_node {
	Window *wnd;
	uint32_t rsm;
	uint32_t wsm;
	uint32_t nwsum; //new width sum
	uint32_t lw;
	uint32_t nlw;
	uint8_t skip;
};
