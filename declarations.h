
enum { NetSupported, NetWMName, NetWMState, NetWMFullscreen, NetActiveWindow, NetWMWindowType, 
	NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum {
	AT_NORTH_EDGE =	1<<0,
	AT_EAST_EDGE =	1<<1,
	AT_SOUTH_EDGE =	1<<2,
	AT_WEST_EDGE =	1<<3,
	BREAK_POINT =	1<<4
};

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

#define SETUP_NUM_ATOMS (WMLast + NetLast)

void map_request(xcb_generic_event_t *ev);
void unmap_notify(xcb_generic_event_t *ev);
void key_press(xcb_generic_event_t *ev);
void configure_request(xcb_generic_event_t *ev);
void enter_notify(xcb_generic_event_t *ev);
uint32_t calc_width_east(Window *current, Window *lim_window);
uint32_t calc_width_west(Window *current, Window *lim_window);
uint32_t calc_height_south(Window *current, Window *lim_window);
uint32_t calc_height_north(Window *current, Window *lim_window);
void focus_in(xcb_generic_event_t *ev);
void focus_out(xcb_generic_event_t *ev);
Window* bfs_search(Window *current, xcb_window_t key);
void insert_window_after(Window *root, xcb_window_t after_which, xcb_window_t new_window);

static void (*handler[XCB_GE_GENERIC]) (xcb_generic_event_t *) = {
	[XCB_BUTTON_PRESS] = NULL,
	[XCB_CONFIGURE_REQUEST] = configure_request,
	[XCB_CONFIGURE_NOTIFY] = NULL,
	[XCB_DESTROY_NOTIFY] = NULL,
	[XCB_ENTER_NOTIFY] = enter_notify,
	[XCB_EXPOSE] = NULL,
	[XCB_FOCUS_IN] = focus_in,
	[XCB_FOCUS_OUT] = focus_out,
	[XCB_KEY_PRESS] = key_press,
	[XCB_MAP_NOTIFY] = NULL,
	[XCB_MAP_REQUEST] = map_request,
	[XCB_PROPERTY_NOTIFY] = NULL,
	[XCB_UNMAP_NOTIFY] = unmap_notify
};

static const struct {
	const char* name;
	int number;
	char isnet;
} setup_atoms[SETUP_NUM_ATOMS] = {
	{ "_NET_SUPPORTED",		NetSupported, 1 },
	{ "_NET_WM_NAME",		NetWMName, 1 },
	{ "_NET_WM_STATE",		NetWMState, 1 },
	{ "_NET_WM_STATE_FULLSCREEN",	NetWMFullscreen, 1 },
	{ "_NET_ACTIVE_WINDOW",		NetActiveWindow, 1 },
	{ "_NET_WM_WINDOW_TYPE",	NetWMWindowType, 1 },
	{ "_NET_WM_WINDOW_TYPE_DIALOG",	NetWMWindowTypeDialog, 1 },
	{ "_NET_CLIENT_LIST",		NetClientList, 1 },
	{ "WM_PROTOCOLS",		WMProtocols, 0 },
	{ "WM_DELETE_WINDOW",		WMDelete, 0 },
	{ "WM_STATE",			WMState, 0 }, 
	{ "WM_TAKE_FOCUS",		WMTakeFocus, 0 } 
};

static xcb_atom_t wmatom[WMLast], netatom[NetLast];
static uint32_t values[3];
static xcb_connection_t *connection;
static xcb_screen_t *screen;
static xcb_drawable_t win;
static xcb_drawable_t root;
static xcb_generic_event_t *ev;
static xcb_get_geometry_reply_t *geom;
static xcb_get_window_attributes_reply_t* attr;
static xcb_query_tree_reply_t *query_tree;
static uint32_t infocus_color, outfocus_color;
static char split_mode = 'h';
static xcb_window_t focused_window;
static Window *Root, *Current, *Current2, *Prev; // "Root" here refers not to the X root window, but the root of the binary tree
static xcb_gcontext_t gc;
static xcb_icccm_wm_hints_t wm_hints;
