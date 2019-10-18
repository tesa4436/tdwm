#include <xcb/xcb.h>

enum { EAST, SOUTH, NORTH, WEST};
enum { X, Y, WIDTH, HEIGHT, GROUPWIDTH, GROUPHEIGHT};

typedef struct win {
	xcb_window_t wnd; // the X11 window id
	uint32_t dim[6]; //dimensions, accessed by using sym constants above
	struct win *n[4]; //next
} Window;

struct stack_node {
	Window *wnd;
	uint32_t rsm; // remainder sum, 
	uint32_t wsm; // width sum
	uint32_t nwsum; //new width sum
	uint32_t lw; // local_width (the resized group's width before resizing)
	uint32_t nlw; // new_local_width (TODO change this bad name)
	uint8_t skip;
};
