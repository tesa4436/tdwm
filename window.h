#include <xcb/xcb.h>

enum { EAST, SOUTH, NORTH, WEST };
enum { X, Y, WIDTH, HEIGHT};

typedef struct win {
	xcb_window_t window;
	uint32_t dimensions[4];
	struct win *next[4];
} Window;

struct stack_node {
	Window *win;
	uint32_t remsum;
	uint32_t widthsum;
};
