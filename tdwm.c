#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
//#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>
#include "window.h"
#include "declarations.h"
#define BORDER_WIDTH 0

uint32_t calc_length(Window *current, Window *lim_window, unsigned char flag)
{
	if (!current)
		return 0;
	uint32_t sum = 0;
	unsigned char off1, off2, off3, off4;
	switch (flag) {
		case EAST: {
			off1 = WIDTH, off2 = EAST, off3 = HEIGHT, off4 = SOUTH;
		} break;
		case SOUTH: {
			off1 = HEIGHT, off2 = SOUTH, off3 = WIDTH, off4 = EAST;
		} break;
		case WEST: {
			off1 = WIDTH, off2 = WEST, off3 = HEIGHT, off4 = SOUTH;
		} break;
		case NORTH: {
			off1 = HEIGHT, off2 = NORTH, off3 = WIDTH, off4 = EAST;
		} break;
		default: return 0;
	}
	Window *current2;
	sum += current->dimensions[off1] + BORDER_WIDTH*2;
	while ((current = current->next[off2])) {
		current2 = current;
		if (lim_window) {
			if (current->dimensions[off3] > lim_window->dimensions[off3])
				break;
			/*
			while ((current2 = current2->next[off4])) {
				if(current2->dimensions[off3 - 2] >= lim_window->dimensions[off3 - 2] + lim_window->dimensions[off3])
					return sum;
			}
			*/
		}
		sum += current->dimensions[off1] + BORDER_WIDTH*2;
	}
	return sum;
}

void change_x_and_width(Window *current, uint32_t x, int32_t width, Window *lim_window, uint32_t local_width, unsigned char flag)
{
	if (!current)
		return;
	struct stack_node stack[3];
	size_t wincount = 1;
	uint32_t rem, oldwidth, widthsum, new_local_width;
	int64_t remsum, oldremsum;
	unsigned char x_or_y, height_or_width, west_or_north, north_or_west, east_or_south;
	uint16_t mask1, mask2;
	if (flag == HORIZONTAL) {
		x_or_y = X, height_or_width = HEIGHT, west_or_north = WEST, north_or_west = NORTH, east_or_south = EAST;
		mask1 = XCB_CONFIG_WINDOW_X, mask2 = XCB_CONFIG_WINDOW_WIDTH;
	} else if (flag == VERTICAL) {
		x_or_y = Y, height_or_width = WIDTH, west_or_north = NORTH, north_or_west = WEST, east_or_south = SOUTH;
		mask1 = XCB_CONFIG_WINDOW_Y, mask2 = XCB_CONFIG_WINDOW_HEIGHT;
	} else return;

	if (lim_window->dimensions[height_or_width - 2] + lim_window->dimensions[height_or_width] + (BORDER_WIDTH * 2) <= current->dimensions[height_or_width - 2])
		return;
	if (width > 0)
		new_local_width = local_width - width; // means that the window group's width will increase
	else
		new_local_width = local_width + (width * -1); // same, but decreases
	stack[0].win = current;
	stack[0].remsum = 0;
	stack[0].widthsum = 0;
	while (wincount) {
		current = stack[wincount - 1].win;
		remsum = stack[wincount - 1].remsum;
		widthsum = stack[wincount - 1].widthsum;
		widthsum += current->dimensions[x_or_y + 2];
		rem = (current->dimensions[x_or_y + 2] * local_width) % new_local_width; 
		remsum += rem;
		oldremsum = remsum;
		oldwidth = current->dimensions[x_or_y + 2];
		current->dimensions[x_or_y + 2] = (current->dimensions[x_or_y + 2] * local_width) / new_local_width;
		wincount--;

		if (current->next[west_or_north])
			current->dimensions[x_or_y] = current->next[west_or_north]->dimensions[x_or_y] + current->next[west_or_north]->dimensions[x_or_y + 2] + BORDER_WIDTH*2;
		else if (current->next[north_or_west])
			current->dimensions[x_or_y] = current->next[north_or_west]->dimensions[x_or_y];
		else
			current->dimensions[x_or_y] = x;
		if (!current->next[east_or_south] && lim_window->next[east_or_south] &&
			lim_window->next[east_or_south]->dimensions[x_or_y] + new_local_width > current->dimensions[x_or_y] + current->dimensions[x_or_y + 2]) {
			uint32_t operand = ((new_local_width - widthsum) * local_width) % new_local_width + 1;
			uint32_t operand2 = (lim_window->dimensions[x_or_y + 2] * local_width) % new_local_width;
			if (remsum >= operand) {
				if (!((remsum - operand) % new_local_width))
					remsum -= operand; 
				else	remsum -= operand2;
			} else	remsum -= operand2;
		}
		if (current->next[EAST]) {
			if (flag == HORIZONTAL && current->next[EAST]->dimensions[X] < lim_window->dimensions[X] + local_width) {
				wincount++;
				stack[wincount - 1].remsum = remsum;
				stack[wincount - 1].widthsum = widthsum;
				stack[wincount - 1].win = current->next[EAST];
			} else if (flag == VERTICAL && (lim_window->dimensions[X] + lim_window->dimensions[WIDTH] + (BORDER_WIDTH * 2) > current->next[EAST]->dimensions[X])) {
				wincount++;
				stack[wincount - 1].remsum = oldremsum - rem;
				stack[wincount - 1].widthsum = widthsum - oldwidth;
				stack[wincount - 1].win = current->next[EAST];
			} else if (flag == HORIZONTAL)
				current->dimensions[x_or_y + 2] += remsum / new_local_width;
		} else if (flag == HORIZONTAL)
			current->dimensions[x_or_y + 2] += remsum / new_local_width;
		if (current->next[SOUTH]) {
			if (flag == HORIZONTAL && (lim_window->dimensions[Y] + lim_window->dimensions[HEIGHT] + (BORDER_WIDTH * 2) > current->next[SOUTH]->dimensions[Y])) {
				wincount++;
				stack[wincount - 1].remsum = oldremsum - rem;
				stack[wincount - 1].widthsum = widthsum - oldwidth;
				stack[wincount - 1].win = current->next[SOUTH];
			} else if (flag == VERTICAL && current->next[SOUTH]->dimensions[Y] < lim_window->dimensions[Y] + local_width) {
				wincount++;
				stack[wincount - 1].remsum = remsum;
				stack[wincount - 1].widthsum = widthsum;
				stack[wincount - 1].win = current->next[SOUTH];
			} else if (flag == VERTICAL)
				current->dimensions[x_or_y + 2] += remsum / new_local_width;
		} else if (flag == VERTICAL)
			current->dimensions[x_or_y + 2] += remsum / new_local_width;
		xcb_configure_window(connection, current->window, mask1, current->dimensions + x_or_y);
		xcb_configure_window(connection, current->window, mask2, current->dimensions + x_or_y + 2);
	}
}


void insert_window_after(Window *tree_root, xcb_window_t after_which, xcb_window_t new_window) // this function uses a bunch of global variables, TODO make local
{
	Window Temp;
	unsigned char south_or_east, north_or_west, height_or_width, width_or_height;
	uint16_t mask2;
	xcb_get_geometry_cookie_t root_geom_cookie;
	if (after_which == root) {
		root_geom_cookie = xcb_get_geometry(connection, root);
		Root = malloc(sizeof(Window));
		if (!Root) {
			fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
			exit(1);
		}
		Root->window = new_window;
		for (int i = 0; i < 4; i++)
			Root->next[i] = NULL;
		Current = Root; // Current is global, probably should be changed to local
		geom = xcb_get_geometry_reply(connection, root_geom_cookie, NULL);
		Root->dimensions[WIDTH] = geom->width - BORDER_WIDTH*2;
		Root->dimensions[HEIGHT] = geom->height - BORDER_WIDTH*2;
		Root->dimensions[X] = 0;
		Root->dimensions[Y] = 0;
		xcb_configure_window(connection, Root->window,	XCB_CONFIG_WINDOW_X |
								XCB_CONFIG_WINDOW_Y |
								XCB_CONFIG_WINDOW_WIDTH |
								XCB_CONFIG_WINDOW_HEIGHT,
								Root->dimensions);
	} else {
		Current = bfs_search(tree_root, after_which);
		if (!Current)
			return;
		Temp = *Current;
		Prev = Current;
		Window *arr[4];

		if (split_mode == 'v') {
			south_or_east = SOUTH;
			north_or_west = NORTH;
			height_or_width = HEIGHT;
			width_or_height = WIDTH;
			mask2 = XCB_CONFIG_WINDOW_HEIGHT;
			arr[EAST] = NULL;
			arr[SOUTH] = Temp.next[south_or_east];
			arr[NORTH] = Prev;
			arr[WEST] = NULL;
		} else {
			south_or_east= EAST;
			north_or_west = WEST;
			height_or_width = WIDTH;
			width_or_height = HEIGHT;
			mask2 = XCB_CONFIG_WINDOW_WIDTH;
			arr[EAST] = Temp.next[south_or_east];
			arr[SOUTH] = NULL;
			arr[NORTH] = NULL;
			arr[WEST] = Prev;
		}
		Current->next[south_or_east] = malloc(sizeof(Window));
		if (!Current->next[south_or_east]) {
			fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
			exit(2);
		}
		Current->next[south_or_east]->window = new_window;
		printf("added %u to tree, %u\n", new_window, new_window % 16);
		for (int i = 0; i < 4; i++)
			Current->next[south_or_east]->next[i] = arr[i];
		if (Current->next[south_or_east]->next[south_or_east])
			Current->next[south_or_east]->next[south_or_east]->next[north_or_west] = Current->next[south_or_east];
		Current->next[south_or_east]->dimensions[height_or_width] = Current->dimensions[height_or_width] % 2;
		Current->dimensions[height_or_width] = (Current->dimensions[height_or_width] + BORDER_WIDTH * 2) / 2 - BORDER_WIDTH * 2; // calc current's width or height, depends on offset3
		Current->next[south_or_east]->dimensions[north_or_west - 2] = Current->dimensions[north_or_west - 2]; // either east or south neighbour's x or y
		Current->next[south_or_east]->dimensions[height_or_width - 2] = Current->dimensions[height_or_width - 2] + ((Current->dimensions[height_or_width] + BORDER_WIDTH * 2)); // y or x
		Current->next[south_or_east]->dimensions[height_or_width] += Current->dimensions[height_or_width];
		Current->next[south_or_east]->dimensions[width_or_height] = Current->dimensions[width_or_height];
		xcb_configure_window(connection, Current->window, mask2, Current->dimensions + height_or_width);
		xcb_configure_window(connection, Current->next[south_or_east]->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
										XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT , Current->next[south_or_east]->dimensions);
	}
}

Window* bfs_search(Window *current, xcb_window_t key)
{
	if (!current)
		return NULL;
	Window *east = NULL, *south = NULL;
	if (current->window == key)
		return current;
	if (current->next[EAST])
		east = bfs_search(current->next[EAST], key);
	if (east)
		return east;
	if (current->next[SOUTH])
		south = bfs_search(current->next[SOUTH], key);
	return south;
}

void print_node(Window *root, xcb_window_t win)
{
	Window *window = bfs_search(root, win);
	if (window) {
		if (window == Root)
			printf("----ROOT----\n");
		printf("node %d\n",window->window);
		
		if (window->next[NORTH])
			printf("north %d ", window->next[NORTH]->window);
		if (window->next[EAST])
			printf("east %d ", window->next[EAST]->window);
		if (window->next[SOUTH])
			printf("south %d ", window->next[SOUTH]->window);
		if (window->next[WEST])
			printf("west %d\n", window->next[WEST]->window);
		printf("\n%u %u %u %u\n\n", window->dimensions[X], window->dimensions[Y], window->dimensions[WIDTH], window->dimensions[HEIGHT]);
	}
}

void map_request(xcb_generic_event_t *ev)
{
	xcb_map_request_event_t *mapreq_ev = (xcb_map_request_event_t *) ev;
	xcb_get_property_cookie_t wm_hints_cookie = xcb_icccm_get_wm_hints(connection, mapreq_ev->window);
	xcb_window_t prop = 0;
	values[0] = BORDER_WIDTH;
	geom = NULL;
	xcb_configure_window(connection, mapreq_ev->window, XCB_CONFIG_WINDOW_BORDER_WIDTH, values);
	if (xcb_icccm_get_wm_transient_for_reply(connection, xcb_icccm_get_wm_transient_for (connection, mapreq_ev->window), &prop, NULL)) {
		Current = bfs_search(Root, prop);
		geom = xcb_get_geometry_reply(connection, xcb_get_geometry(connection, mapreq_ev->window), NULL);
		if ((Current->dimensions[WIDTH] > geom->width) && (Current->dimensions[HEIGHT] > geom->height)) {
			values[0] = Current->dimensions[X] + Current->dimensions[WIDTH]/2 - geom->width/2;
			values[1] = Current->dimensions[Y] + Current->dimensions[HEIGHT]/2 - geom->height/2;
		} else {
			values[0] = Root->dimensions[WIDTH]/2;
			values[1] = Root->dimensions[HEIGHT]/2;
		}
		xcb_configure_window(connection, mapreq_ev->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	} else
		insert_window_after(Root, focused_window, mapreq_ev->window); // insert mapreq_ev->window after focused_window
	if (xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
		xcb_icccm_wm_hints_set_normal(&wm_hints);
		xcb_icccm_set_wm_hints(connection, mapreq_ev->window, &wm_hints);
	}
	values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_FOCUS_CHANGE;
	xcb_change_window_attributes(connection, mapreq_ev->window, XCB_CW_EVENT_MASK, values);			
	xcb_map_window(connection,mapreq_ev->window);
	focused_window = mapreq_ev->window;
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, mapreq_ev->window, XCB_CURRENT_TIME);
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, root, netatom[NetActiveWindow], XCB_WINDOW, 32, 1, &mapreq_ev->window);
	xcb_flush(connection);
	free(geom);
	geom = NULL;
	free(ev);
}

void unmap_notify(xcb_generic_event_t *ev)
{
	char direction_east;
	uint32_t local_width, local_height, local_width_or_height;
	unsigned char off1, off2, off3, off4, off5, off6, off7;
	xcb_unmap_notify_event_t *unmap_ev = (xcb_unmap_notify_event_t *) ev; 
	xcb_get_property_cookie_t wm_hints_cookie = xcb_icccm_get_wm_hints(connection, unmap_ev->window);
	Current = bfs_search(Root, unmap_ev->window);
	if (Current) {
		Window *Temp1 = NULL, *Temp2 = NULL;
		if (Current->next[EAST] && Current->next[SOUTH]) {
			direction_east = (Current->dimensions[HEIGHT] + BORDER_WIDTH*2) < calc_length(Current->next[EAST], NULL, SOUTH) ? 0 : 1;
			if (direction_east) {
				off1 = EAST, off2 = SOUTH, off3 = NORTH, off4 = WEST, off5 = HORIZONTAL, off6 = X, off7 = HEIGHT;
			} else {
				off1 = SOUTH, off2 = EAST, off3 = WEST, off4 = NORTH, off5 = VERTICAL, off6 = Y, off7 = WIDTH;
			}
			local_width_or_height = calc_length(Current, Current, off1);
			Current2 = Current->next[off1];
			while (Current2->next[off2])
				Current2 = Current2->next[off2];	
			Current2->next[off2] = Current->next[off2];
			Current->next[off2]->next[off3] = Current2;
			Current2 = Current->next[off1];
			if (Current->next[off3]) {
				Current->next[off1]->next[off3] = Current->next[off3];
				Current->next[off3]->next[off2] = Current->next[off1];
			} else
				Current->next[off1]->next[off3] = NULL;
			if (Current->next[off4]) {
				Current->next[off1]->next[off4] = Current->next[off4];
				Current->next[off4]->next[off1] = Current->next[off1];
			} else
				Current->next[off1]->next[off4] = NULL;
			change_x_and_width(Current->next[off1], Current->dimensions[off6], Current->dimensions[off6 + 2], Current, local_width_or_height, off5);
			xcb_flush(connection); // a rather awkward place for a resize function call, TODO restructure

			if (Current == Root) {
				Root = direction_east ? Current->next[EAST] : Current->next[SOUTH];
				Root->next[NORTH] = NULL;
				Root->next[WEST] = NULL;
			}
			free(Current);
			Current = NULL;
			if (xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
				xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
				xcb_icccm_set_wm_hints(connection, unmap_ev->window, &wm_hints);
			}
			free(ev);
			return;
		} else if (!Current->next[EAST] && Current->next[SOUTH])
			off1 = SOUTH, off2 = WEST, off3 = NORTH, off4 = EAST, off5 = VERTICAL, off6 = WIDTH, off7 = Y;
		else if (Current->next[EAST] && !Current->next[SOUTH])
			off1 = EAST, off2 = NORTH, off3 = WEST, off4 = SOUTH, off5 = HORIZONTAL, off6 = HEIGHT, off7 = X;
		else {
			Current2 = Current;
			if (Current->next[NORTH])
				off1 = NORTH, off2 = SOUTH, off3 = WIDTH, off4 = VERTICAL, off5 = Y;
			else if (Current->next[WEST])
				off1 = WEST, off2 = EAST, off3 = HEIGHT, off4 = HORIZONTAL, off5 = X;
			else {
				free(Current);
				Current = NULL;
				if (xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
					xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
					xcb_icccm_set_wm_hints(connection, unmap_ev->window, &wm_hints);
				}
				free(ev);
				return;
			}
			local_width_or_height = calc_length(Current, Current, off1);
			Current->next[off1]->next[off2] = NULL;
			while (Current2->next[off1] && Current2->next[off1]->dimensions[off3] <= Current->dimensions[off3])
				Current2 = Current2->next[off1];
			change_x_and_width(Current2, Current2->dimensions[off5], Current->dimensions[off5 + 2], Current, local_width_or_height, off4);
			xcb_flush(connection);
			free(Current);
			Current = NULL;
			if (xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
				xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
				xcb_icccm_set_wm_hints(connection, unmap_ev->window, &wm_hints);
			}
			free(ev);
			return;
		}
		local_width_or_height = calc_length(Current, Current, off1); // TODO these 3 length calculations waste resources, put them in separate scopes, refactoring required
		Current2 = Current;
		Temp1 = Current->next[EAST];
		Temp2 = Current->next[SOUTH];
		Current->next[EAST] = NULL;
		Current->next[SOUTH] = NULL;
		if (Current->next[NORTH])
			local_height = calc_length(Current, Current, NORTH); // or perhaps precalculate the lengths and store them in the nodes?..
		else if (Current->next[WEST])
			local_width = calc_length(Current, Current, WEST);
		Current->next[EAST] = Temp1;
		Current->next[SOUTH] = Temp2;
		if (Current == Root) {
			Root = Current->next[off1];
			Root->next[NORTH] = NULL;
			Root->next[WEST] = NULL;
		} else if (Current->next[off2]) {	
			Current->next[off2]->next[off4] = Current->next[off1];
			Current->next[off1]->next[off3] = NULL;
			Current->next[off1]->next[off2] = Current->next[off2];
		} else {
			Current->next[off3]->next[off1] = Current->next[off1];
			Current->next[off1]->next[off2] = NULL;
			Current->next[off1]->next[off3] = Current->next[off3];
		}
		if ((Current->dimensions[off6] + BORDER_WIDTH*2) >= calc_length(Current->next[off1], NULL, off4))
			change_x_and_width(Current->next[off1], Current->dimensions[off7], Current->dimensions[off7 + 2], Current, local_width_or_height, off5);
		else if (Current->next[NORTH]) {
			Window *Temp = Current->next[NORTH]->next[SOUTH];
			Current->next[NORTH]->next[SOUTH] = NULL;
			while (Current2->next[NORTH] && Current2->next[NORTH]->dimensions[WIDTH] <= Current->dimensions[WIDTH]) {
				Current2 = Current2->next[NORTH];
			}
			change_x_and_width(Current2, Current2->dimensions[Y], Current->dimensions[HEIGHT], Current, local_height, VERTICAL);
			Current->next[NORTH]->next[SOUTH] = Temp;
		} else if (Current->next[WEST]) {
			Window *Temp = Current->next[WEST]->next[EAST];
			Current->next[WEST]->next[EAST] = NULL;
			while (Current2->next[WEST] && Current2->next[WEST]->dimensions[HEIGHT] <= Current->dimensions[HEIGHT]) {
				Current2 = Current2->next[WEST];
			}
			change_x_and_width(Current2, Current2->dimensions[X], Current->dimensions[WIDTH], Current, local_width, HORIZONTAL);
			Current->next[WEST]->next[EAST] = Temp;
		}
		xcb_flush(connection);
		free(Current);
		Current = NULL;
	}
	if (xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
		xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
		xcb_icccm_set_wm_hints(connection, unmap_ev->window, &wm_hints);
	}
	
	free(ev);
}

struct key_mapping xcb_get_keyboard_mapping(xcb_connection_t *connection, const xcb_setup_t *setup)
{
	xcb_get_keyboard_mapping_reply_t *keyboard_mapping = xcb_get_keyboard_mapping_reply(	connection,
												xcb_get_keyboard_mapping(connection,
															setup->min_keycode,
															setup->max_keycode - setup->min_keycode + 1),
												NULL);
	int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;
	int nkeysyms = keyboard_mapping->length;
	xcb_keysym_t *keysyms = (xcb_keysym_t*) (keyboard_mapping + 1);  // `xcb_keycode_t` is just a `typedef u8`, and `xcb_keysym_t` is just a `typedef u32`
	printf("nkeycodes %u  nkeysyms %u  keysyms_per_keycode %u\n\n", nkeycodes, nkeysyms, keyboard_mapping->keysyms_per_keycode);
	for(int keycode_idx = 0; keycode_idx < nkeycodes; keycode_idx++) {
		printf("keycode %3u ", setup->min_keycode + keycode_idx);
			for (int keysym_idx = 0; keysym_idx < keyboard_mapping->keysyms_per_keycode; keysym_idx++) {
				printf(" %c", keysyms[keysym_idx + keycode_idx * keyboard_mapping->keysyms_per_keycode]);
			}
		putchar('\n');
	}
	free(keyboard_mapping);
}

void key_press(xcb_generic_event_t *ev)
{
	xcb_key_press_event_t *key_event = (xcb_key_press_event_t *)ev;
	//printf ("Key pressed in window\n");
	if (key_event->child) {
		win = key_event->child;
		geom = xcb_get_geometry_reply(connection, xcb_get_geometry(connection, win), NULL);
		
		switch(key_event->detail) {
			case 111: { //up arrow key keycode
				values[0] = geom->x + 0;
				values[1] = geom->y - 10;
				xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_flush(connection);
			}
			break;
	
			case 113: { //left arrow key keycode
				values[0] = geom->x - 10;
				values[1] = geom->y + 0;
				xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_flush(connection);
				
			}
			break;
	
			case 114: { //right arrow key keycode
				values[0] = geom->x + 10;
				values[1] = geom->y + 0;
				xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_flush(connection);
				
				/*
				Current = bfs_search(Root, key_event->child);
				Current->width += 10;
				values[0] = Current->width;
				values[1] = Current->height;
				xcb_configure_window(connection, key_event->child, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
				recursive_resize_and_repos_horizontal(Current->east_next, Current->x + Current->width, -10, NULL);
				xcb_flush(connection);
				*/
				
				

			}
			break;

			case 116: { //down arrow key keycode
				values[0] = geom->x + 0;
				values[1] = geom->y + 10;
				xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_flush(connection);
			}
			break;
		
			case 55: {
				split_mode = 'v';
			}
			break;
		
			case 43: {
				split_mode = 'h';
			}
			break;
		}
		free(geom);
		geom = NULL;
	}
	free(ev);
}

void enter_notify(xcb_generic_event_t *ev)
{
	xcb_enter_notify_event_t *enternotf_ev = (xcb_enter_notify_event_t *) ev;
	print_node(Root, enternotf_ev->event);
	Current = bfs_search(Root, enternotf_ev->event);
	focused_window = enternotf_ev->event;
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, enternotf_ev->event, XCB_CURRENT_TIME);
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, root, netatom[NetActiveWindow], XCB_WINDOW, 32, 1, &enternotf_ev->event);
	xcb_flush(connection);
	free(ev);
}

void focus_in(xcb_generic_event_t *ev)
{
	xcb_focus_in_event_t *focusin_ev = (xcb_focus_in_event_t *) ev;
	focused_window = focusin_ev->event;
	Current = bfs_search(Root, focusin_ev->event);
	if (Current)
		xcb_change_window_attributes(connection, Current->window, XCB_CW_BORDER_PIXEL, &infocus_color);
	xcb_flush(connection);
	free(ev);
}

void focus_out(xcb_generic_event_t *ev)
{
	xcb_focus_out_event_t *focusout_ev = (xcb_focus_out_event_t *) ev;
	Current = bfs_search(Root, focusout_ev->event);
	if (Current)
		xcb_change_window_attributes(connection, Current->window, XCB_CW_BORDER_PIXEL, &outfocus_color);
	xcb_flush(connection);
	free(ev);
}

void client_message(xcb_generic_event_t *ev)
{
	xcb_client_message_event_t *client_msg = (xcb_client_message_event_t *) ev;
	printf("client message, window %d ", client_msg->window);
	
	if (client_msg->type == netatom[NetWMState])
		printf(" _NET_WM_STATE\n");
	printf("\n");
	free(ev);
}

void configure_request(xcb_generic_event_t *ev)
{

	xcb_configure_request_event_t *confreq_ev = (xcb_configure_request_event_t *) ev;
	printf("conf %d\n", confreq_ev->window);
	free(ev);
}

uint32_t get_color(uint16_t r, uint16_t g, uint16_t b) // TODO: error checking
{
	xcb_alloc_color_reply_t *color_rep = xcb_alloc_color_reply(connection, xcb_alloc_color(connection, screen->default_colormap, r, g, b), NULL);
	uint32_t color = color_rep->pixel;
	free(color_rep);
	return color;
}

void setup(void)
{
	connection = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(connection)) {
		fprintf(stderr, "tdwm: could not connect to X server, exiting\n");
		exit(1);
	}
	xorg_setup = xcb_get_setup(connection);
	kmapping = xcb_get_keyboard_mapping(connection, xorg_setup);
	screen = xcb_setup_roots_iterator(xorg_setup).data;
	root = screen->root;
	focused_window = root;
	gc = xcb_generate_id(connection);
	values[0] = 1;
	values[1] = XCB_LINE_STYLE_SOLID | XCB_CAP_STYLE_BUTT | XCB_JOIN_STYLE_MITER;
	xcb_create_gc(connection, gc, root, XCB_GC_LINE_WIDTH | XCB_GC_LINE_STYLE | XCB_GC_CAP_STYLE | XCB_GC_JOIN_STYLE, values);
	values[0] = screen->white_pixel;
	values[1] = screen->black_pixel;
	xcb_change_gc(connection, gc, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, values);
	
	// this code that sets up atoms was taken from dwm's code
	xcb_intern_atom_cookie_t atom_cookie[SETUP_NUM_ATOMS];
	for (int i=0; i<SETUP_NUM_ATOMS; i++) {
		atom_cookie[i] = xcb_intern_atom(connection, 0, strlen(setup_atoms[i].name), setup_atoms[i].name);
	}
	for (int i=0; i<SETUP_NUM_ATOMS; i++) {
		xcb_intern_atom_reply_t *reply;
		if ((reply = xcb_intern_atom_reply(connection, atom_cookie[i], NULL))) {
			if (setup_atoms[i].isnet)
				netatom[setup_atoms[i].number] = reply->atom;
			else
				wmatom[setup_atoms[i].number] = reply->atom;
			free(reply);
		}
	}
	infocus_color = get_color((uint16_t) -1, 0, 0);
	outfocus_color = get_color(10000, 10000, 10000);
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, root, netatom[NetSupported], XCB_ATOM, 32, NetLast, (unsigned char*)netatom);
	values[0] = 	XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | 	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
	 		XCB_EVENT_MASK_ENTER_WINDOW | 		XCB_EVENT_MASK_LEAVE_WINDOW |
			XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	xcb_change_window_attributes(connection, root, XCB_CW_EVENT_MASK, values);
	xcb_grab_key(connection, 0, root, XCB_MOD_MASK_1, 116, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(connection, 0, root, XCB_MOD_MASK_1, 111, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(connection, 0, root, XCB_MOD_MASK_1, 113, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(connection, 0, root, XCB_MOD_MASK_1, 114, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(connection, 0, root, XCB_MOD_MASK_1, 55, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC); // letter v
	xcb_grab_key(connection, 0, root, XCB_MOD_MASK_1, 43, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC); // letter h
	//xcb_grab_pointer(connection, 1, root, XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
	
    //xcb_grab_button(connection, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, XCB_MOD_MASK_1);
    //xcb_grab_button(connection, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 3, XCB_MOD_MASK_1);
	xcb_flush(connection);
}

int main(void)
{
	setup();
	for (;;) {
		ev = xcb_wait_for_event(connection);
		if (handler[ev->response_type & ~0x80])
			handler[ev->response_type & ~0x80](ev);
		else free(ev);
	}
	return 0;
}

