#include <xcb/xcb.h>

enum { EAST, SOUTH, NORTH, WEST};
enum { X, Y, WIDTH, HEIGHT, GROUPWIDTH, GROUPHEIGHT};

typedef struct win {
	xcb_window_t window;
	uint32_t dimensions[6];
	struct win *next[4];
} Window;

struct stack_node {
	Window *win;
	uint32_t remsum;
	uint32_t widthsum;
	uint32_t nwsum; //new width sum
	uint32_t local_width;
	uint32_t new_local_width;
	uint8_t skip;
};
