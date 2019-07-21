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
#define SOUTH_MODE 0
#define EAST_MODE 1
#define BORDER_WIDTH 0

uint32_t calc_width_east(Window *current, Window *lim_window)
{
	if(!current)
		return 0;
	uint32_t sum = 0;
	sum+=current->width + BORDER_WIDTH*2;
	while(current->east_next) {
		current = current->east_next;
		if(lim_window)
			if(current->height > lim_window->height)
				break;
		sum+=current->width + BORDER_WIDTH*2;
	}
	return sum;
}

uint32_t calc_height_south(Window *current, Window *lim_window)
{
	if(!current)
		return 0;
	uint32_t sum = 0;
	sum+=current->height + BORDER_WIDTH*2;
	while(current->south_next) {
		current = current->south_next;
		if(lim_window)
			if(current->width > lim_window->width)
				break;
		sum+=current->height + BORDER_WIDTH*2;
	}
	return sum;
}

uint32_t calc_width_west(Window *current, Window *lim_window)
{
	if(!current)
		return 0;
	uint32_t sum = 0;
	sum+=current->width + BORDER_WIDTH*2;
	while(current->west_prev) {
		current = current->west_prev;
		if(lim_window)
			if(current->height > lim_window->height)
				break;
		sum+=current->width + BORDER_WIDTH*2;
	}
	return sum;
}

uint32_t calc_height_north(Window *current, Window *lim_window)
{
	if(!current)
		return 0;
	uint32_t sum = 0;
	sum+=current->height + BORDER_WIDTH*2;
	while(current->north_prev) {
		current = current->north_prev;
		if(lim_window)
			if(current->width > lim_window->width)
				break;
		sum+=current->height + BORDER_WIDTH*2;
	}
	return sum;
}

void bfs_count(Window *current, uint32_t *counter)
{
	if(!current)
		return;
	(*counter)++;
	if(current->east_next)
		bfs_count(current->east_next, counter);

}
void recursive_resize_and_repos_vertical(Window *current, uint32_t y, Window *lim_window, uint32_t local_height)
{		
	if(!current || (lim_window ? (((lim_window->x + lim_window->width + BORDER_WIDTH*2) == current->x) ? 1 : 0 ) || (current->width > lim_window->width) : 0)) {
		return;
	}
	current->y = y;
	current->height = (current->height * (lim_window->height + local_height)) / local_height;
	xcb_configure_window(connection, current->window, XCB_CONFIG_WINDOW_Y, &current->y);
	xcb_configure_window(connection, current->window, XCB_CONFIG_WINDOW_HEIGHT, &current->height);
	recursive_resize_and_repos_vertical(current->east_next, y, lim_window, local_height);
	recursive_resize_and_repos_vertical(current->south_next, current->y + current->height + BORDER_WIDTH*2, lim_window, local_height);
}

void recursive_resize_and_repos_horizontal(Window *current, uint32_t x, Window *lim_window, uint32_t local_width)
{
	if(!current || (lim_window ? (((lim_window->y + lim_window->height + BORDER_WIDTH*2) == current->y) ? 1 : 0 ) || (current->height > lim_window->height) : 0)) {
		return;
	}
	current->x = x;
	printf("%u %u BOI\n", (current->width * (lim_window->width + local_width)), local_width);
	current->width = (current->width * (lim_window->width + local_width)) / local_width;
	xcb_configure_window(connection, current->window, XCB_CONFIG_WINDOW_X, &current->x);
	xcb_configure_window(connection, current->window, XCB_CONFIG_WINDOW_WIDTH, &current->width);
	recursive_resize_and_repos_horizontal(current->east_next, current->x + current->width + BORDER_WIDTH*2, lim_window, local_width);		
	recursive_resize_and_repos_horizontal(current->south_next, current->x, lim_window, local_width);
}

void change_x_and_width(Window *current, uint32_t x, Window *lim_window, uint32_t local_width)
{
	if(!current)
		return;
	Window **stack = malloc(sizeof(Window*) * STACK_SIZE);
	uint32_t *rems_stack = calloc(sizeof(uint32_t), STACK_SIZE);
	if(!stack || !rems_stack)
		return;
	size_t stack_size = STACK_SIZE;
	size_t wincount = 1;
	size_t remsum;
	stack[0] = current;
	while(wincount) {
		current = stack[wincount - 1];
		remsum = rems_stack[wincount - 1];
		if((lim_window ? (((lim_window->y + lim_window->height + BORDER_WIDTH*2) == current->y) ? 1 : 0 ) || (current->height > lim_window->height) : 0))
			break;
		if(current->west_prev)
			current->x = current->west_prev->x + current->west_prev->width + BORDER_WIDTH*2;
		else if(current->north_prev)
			current->x = current->north_prev->x;
		else
			current->x = x;
		remsum += (current->width * (lim_window->width + local_width)) % local_width; //TODO 
		current->width = (current->width * (lim_window->width + local_width)) / local_width;
		wincount--;
		xcb_configure_window(connection, current->window, XCB_CONFIG_WINDOW_X, &current->x);
		xcb_configure_window(connection, current->window, XCB_CONFIG_WINDOW_WIDTH, &current->width);
		if(current->east_next) {
			wincount++;
			if(wincount == stack_size) {
				stack_size *= 2;
				stack = realloc(stack, stack_size);
				rems_stack = realloc(rems_stack, stack_size);
				if(!stack || !rems_stack)
					break;
				memset(rems_stack + (stack_size/2), 0, stack_size/2);
			}
			rems_stack[wincount - 1] = remsum;
			stack[wincount - 1] = current->east_next;
		}
		if(current->south_next) {
			wincount++;
			if(wincount == stack_size) {
				stack_size *= 2;
				stack = realloc(stack, stack_size);
				rems_stack = realloc(rems_stack, stack_size);
				if(!stack || !rems_stack)
					break;
				memset(rems_stack + (stack_size/2), 0, stack_size/2);
			}
			rems_stack[wincount - 1] = remsum;
			stack[wincount - 1] = current->south_next;
		}
		if(!current->east_next)
			printf("rem sum pls %d %lu %u %lu %lu\n", current->window, remsum, local_width, remsum / local_width, remsum % local_width);
	}
	free(stack);
	free(rems_stack);
}


void insert_window_after(Window *tree_root, xcb_window_t after_which, xcb_window_t new_window) // this function uses a bunch of global variables, TODO make local
{
	xcb_get_geometry_cookie_t focus_geom_cookie = xcb_get_geometry(connection, focused_window);
	Window Temp;
	if(after_which == root) {
		xcb_get_geometry_cookie_t root_geom_cookie = xcb_get_geometry(connection, root);
		Root = malloc(sizeof(Window));
		if(!Root) {
			fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
			exit(1);
		}
		Root->window = new_window;
		Root->north_prev = NULL;
		Root->south_next = NULL;
		Root->east_next = NULL;
		Root->west_prev = NULL;
		Root->prev_x = 0;
		Root->prev_y = 0;
		Root->prev_width = 0;
		Root->prev_height = 0;
		Current = Root; // Current is global, probably should be changed to local
		geom = xcb_get_geometry_reply(connection, root_geom_cookie, NULL);
		values[0] = geom->width - BORDER_WIDTH*2;
		values[1] = geom->height - BORDER_WIDTH*2;
		Root->width = geom->width - BORDER_WIDTH*2;
		Root->height = geom->height - BORDER_WIDTH*2;
		Root->x = 0;
		Root->y = 0;
		xcb_configure_window(connection, Root->window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	} else {
		Current = bfs_search(tree_root, after_which);
		if(!Current)
			return;

		Temp = *Current;
		Prev = Current;

		if(split_mode == 'v') {
			Current->south_next = malloc(sizeof(Window));
			if(!Current->south_next) {
				fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
				exit(2);
			}
			Current = Current->south_next;
			Current->window = new_window;
			Current->north_prev = Prev;
			Current->west_prev = NULL;
			Current->east_next = NULL;
			Current->south_next = Temp.south_next;
			Prev->south_next = Current;
			if(Current->south_next)
				Current->south_next->north_prev = Current;
			geom = xcb_get_geometry_reply(connection, focus_geom_cookie, NULL);
			values[0] = geom->width;
			values[1] = (geom->height + BORDER_WIDTH*2) / 2 - BORDER_WIDTH*2;
			xcb_configure_window(connection, Prev->window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
			Prev->width = values[0];
			Prev->height = values[1];
			values[0] = geom->x;
			values[1] = geom->y + ((geom->height + BORDER_WIDTH*2) / 2 - BORDER_WIDTH*2) + BORDER_WIDTH*2;
			xcb_configure_window(connection, Current->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
			Current->x = values[0];
			Current->y = values[1];
			values[0] = geom->width;
			values[1] = Prev->height;
			if(geom->height%2)
				values[1]++;
			xcb_configure_window(connection, Current->window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
			Current->width = values[0];
			Current->height = values[1];
		} else {
			Current->east_next = malloc(sizeof(Window));
			if(!Current->east_next) {
				fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
				exit(2);
			}
			Current = Current->east_next;
			Current->window = new_window;
			Current->north_prev = NULL;
			Current->west_prev = Prev;
			Current->east_next = Temp.east_next;
			Current->south_next = NULL;
			Prev->east_next = Current;
			if(Current->east_next)
				Current->east_next->west_prev = Current;
			geom = xcb_get_geometry_reply(connection, focus_geom_cookie, NULL);
			values[0] = (geom->width + BORDER_WIDTH*2) / 2 - BORDER_WIDTH*2;
			values[1] = geom->height;
			xcb_configure_window(connection, Prev->window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
			Prev->width = values[0];
			Prev->height = values[1];
			values[0] = geom->x + ((geom->width + BORDER_WIDTH*2) / 2 - BORDER_WIDTH*2) + BORDER_WIDTH*2;
			values[1] = geom->y;
			xcb_configure_window(connection, Current->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
			Current->x = values[0];
			Current->y = values[1];
			values[0] = Prev->width;
			if(geom->width%2)
				values[0]++;
			values[1] = geom->height;
			xcb_configure_window(connection, Current->window, XCB_CONFIG_WINDOW_WIDTH |XCB_CONFIG_WINDOW_HEIGHT, values);
			Current->width = values[0];
			Current->height = values[1];
		}
		Current->prev_width = Prev->width;
		Current->prev_height = Prev->height;
		Current->prev_x = Prev->prev_x;
		Current->prev_y = Prev->prev_y;
	}
}

Window* bfs_search(Window *current, xcb_window_t key)
{
	if(!current)
		return NULL;
	Window *east = NULL, *south = NULL;
	if(current->window == key)
		return current;
	if(current->east_next)
		east = bfs_search(current->east_next, key);
	if(east)
		return east;
	if(current->south_next)
		south = bfs_search(current->south_next, key);
	return south;
}

void print_node(Window *root, xcb_window_t win)
{
	Window *window = bfs_search(root, win);
	if(window) {
		if(window == Root)
			printf("----ROOT----\n");
		printf("node %d\n",window->window);
		
		if(window->north_prev)
			printf("north %d ", window->north_prev->window);
		if(window->east_next)
			printf("east %d ", window->east_next->window);
		if(window->south_next)
			printf("south %d ", window->south_next->window);
		if(window->west_prev)
			printf("west %d\n", window->west_prev->window);
		printf("\n%u %u %u %u\n\n", window->x, window->y, window->width, window->height);
	}
}

void map_request(xcb_generic_event_t *ev)
{
	xcb_map_request_event_t *mapreq_ev = (xcb_map_request_event_t *) ev;
	xcb_get_property_cookie_t wm_hints_cookie = xcb_icccm_get_wm_hints(connection, mapreq_ev->window);
	xcb_window_t prop = 0;
	values[0] = BORDER_WIDTH;
	xcb_configure_window(connection, mapreq_ev->window, XCB_CONFIG_WINDOW_BORDER_WIDTH, values);
	if(xcb_icccm_get_wm_transient_for_reply(connection, xcb_icccm_get_wm_transient_for(connection, mapreq_ev->window), &prop, NULL)) {
		Current = bfs_search(Root, prop);
		geom = xcb_get_geometry_reply(connection, xcb_get_geometry(connection, mapreq_ev->window), NULL);
		if((Current->width > geom->width) && (Current->height > geom->height)) {
			values[0] = Current->x + Current->width/2 - geom->width/2;
			values[1] = Current->y + Current->height/2 - geom->height/2;
		} else {
			values[0] = Root->width/2;
			values[1] = Root->height/2;
		}
		xcb_configure_window(connection, mapreq_ev->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	} else
		insert_window_after(Root, focused_window, mapreq_ev->window); // insert mapreq_ev->window after focused_window
	if(xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
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
	if(Current)
		free(geom);
	free(ev);
}

void unmap_notify(xcb_generic_event_t *ev)
{
	xcb_unmap_notify_event_t *unmap_ev = (xcb_unmap_notify_event_t *) ev; 
	xcb_get_property_cookie_t wm_hints_cookie = xcb_icccm_get_wm_hints(connection, unmap_ev->window);
	uint32_t counter = 0;
	
	Current = bfs_search(Root, unmap_ev->window);
	if(Current) {
		char direction_east;
		double mul;
		uint32_t remainder, local_width, local_height;
		Window *Temp1, *Temp2;
		if(Current->east_next && Current->south_next) {
			direction_east = (Current->height + BORDER_WIDTH*2) < calc_height_south(Current->east_next, NULL) ? 0 : 1;
			if(direction_east) {
				Current2 = Current->east_next;
				while(Current2->south_next)
					Current2 = Current2->south_next;	
				change_x_and_width(Current->east_next, Current->x, Current, calc_width_east(Current->east_next, Current));
				xcb_flush(connection);
				Current2->south_next = Current->south_next;
				Current->south_next->north_prev = Current2;

				if(Current->north_prev) {
					Current->east_next->north_prev = Current->north_prev;
					Current->north_prev->south_next = Current->east_next;
				} else
					Current->east_next->north_prev = NULL;
				if(Current->west_prev) {
					Current->east_next->west_prev = Current->west_prev;
					Current->west_prev->east_next = Current->east_next;
				} else
					Current->east_next->west_prev = NULL;
			} else {
				Current2 = Current->south_next;
				while(Current2->east_next)
					Current2 = Current2->east_next;	
				recursive_resize_and_repos_vertical(Current->south_next, Current->y, Current, calc_height_south(Current->south_next, Current));
				xcb_flush(connection);
				Current2->east_next = Current->east_next;
				Current->east_next->west_prev = Current2;
				if(Current->west_prev) {
					Current->south_next->west_prev = Current->west_prev;
					Current->west_prev->east_next = Current->south_next;
				} else
					Current->south_next->west_prev = NULL;
				if(Current->north_prev) {
					Current->south_next->north_prev = Current->north_prev;
					Current->north_prev->south_next = Current->south_next;
				} else
					Current->south_next->north_prev = NULL;
			}
			
			if(Current == Root) {
				Root = direction_east ? Current->east_next : Current->south_next;
				Root->north_prev = NULL;
				Root->west_prev = NULL;
			}
		} else if(!Current->east_next && Current->south_next) {
			Current2 = Current;
			Temp1 = Current->east_next;
			Temp2 = Current->south_next;
			Current->east_next = NULL;
			Current->south_next = NULL;
			local_height = calc_height_north(Current->north_prev, Current);
			local_width = calc_width_west(Current->west_prev, Current);
			printf("locals: %d %d\n", local_width, local_height);
			Current->east_next = Temp1;
			Current->south_next = Temp2;
			if(Current == Root) {
				Root = Current->south_next;
				Root->north_prev = NULL;
				Root->west_prev = NULL;
			} else if(Current->west_prev) {	
				Current->west_prev->east_next = Current->south_next;
				Current->south_next->north_prev = NULL;
				Current->south_next->west_prev = Current->west_prev;
			} else {
				Current->north_prev->south_next = Current->south_next;
				Current->south_next->west_prev = NULL;
				Current->south_next->north_prev = Current->north_prev;
			}
			if((Current->width + BORDER_WIDTH*2) >= calc_width_east(Current->south_next, NULL))
				recursive_resize_and_repos_vertical(Current->south_next, Current->y, Current, calc_height_south(Current->south_next, Current));
			else if(Current->north_prev) {
				Window *Temp = Current->north_prev->south_next;
				Current->north_prev->south_next = NULL;
				while(Current2->north_prev && Current2->north_prev->width <= Current->width) {
					Current2 = Current2->north_prev;
				}
				recursive_resize_and_repos_vertical(Current2, Current2->y, Current, local_height);
				Current->north_prev->south_next = Temp;
			} else if(Current->west_prev) {
				Window *Temp = Current->west_prev->east_next;
				Current->west_prev->east_next = NULL;
				while(Current2->west_prev && Current2->west_prev->height <= Current->height) {
					Current2 = Current2->west_prev;
				}
				change_x_and_width(Current2, Current2->x, Current, local_width);
				Current->west_prev->east_next = Temp;
			}
			xcb_flush(connection);
		} else if(Current->east_next && !Current->south_next) {
			Current2 = Current;
			Temp1 = Current->east_next;
			Temp2 = Current->south_next;
			Current->east_next = NULL;
			Current->south_next = NULL;
			local_height = calc_height_north(Current->north_prev, Current);
			local_width = calc_width_west(Current->west_prev, Current);
			Current->east_next = Temp1;
			Current->south_next = Temp2;
			if(Current == Root) {
				Root = Current->east_next;
				Root->north_prev = NULL;
				Root->west_prev = NULL;
			} else if(Current->north_prev) {
				Current->north_prev->south_next = Current->east_next;
				Current->east_next->west_prev = NULL;
				Current->east_next->north_prev = Current->north_prev;
			} else {
				Current->west_prev->east_next = Current->east_next;
				Current->east_next->north_prev = NULL;
				Current->east_next->west_prev = Current->west_prev;
			}
			if((Current->height + BORDER_WIDTH*2) >= calc_height_south(Current->east_next, NULL))
				change_x_and_width(Current->east_next, Current->x, Current, calc_width_east(Current->east_next, Current));
			else if(Current->north_prev) {
				Window *Temp = Current->north_prev->south_next;
				Current->north_prev->south_next = NULL;
				while(Current2->north_prev && Current2->north_prev->width <= Current->width) {
					Current2 = Current2->north_prev;
				}
				recursive_resize_and_repos_vertical(Current2, Current2->y, Current, local_height);
				Current->north_prev->south_next = Temp;
			} else if(Current->west_prev) {
				Window *Temp = Current->west_prev->east_next;
				Current->west_prev->east_next = NULL;
				while(Current2->west_prev && Current2->west_prev->height <= Current->height) {
					Current2 = Current2->west_prev;
				}
				change_x_and_width(Current2, Current2->x, Current, local_width);
				Current->west_prev->east_next = Temp;
			}
			xcb_flush(connection);
		} else {
			Current2 = Current;
			if(Current->north_prev) {
				Current->north_prev->south_next = NULL;
				while(Current2->north_prev && Current2->north_prev->width <= Current->width) {
					Current2 = Current2->north_prev;
				}
				recursive_resize_and_repos_vertical(Current2, Current2->y, Current, calc_height_north(Current->north_prev, Current));
			}
			if(Current->west_prev) {
				Current->west_prev->east_next = NULL;
				while(Current2->west_prev && Current2->west_prev->height <= Current->height) {
					Current2 = Current2->west_prev;
				}
				change_x_and_width(Current2, Current2->x, Current, calc_width_west(Current->west_prev, Current));
			}
			xcb_flush(connection);
		}
		free(Current);
	}
	
	if(xcb_icccm_get_wm_hints_reply(connection, wm_hints_cookie, &wm_hints, NULL)) {
		xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
		xcb_icccm_set_wm_hints(connection, unmap_ev->window, &wm_hints);
	}
	
	free(ev);
}

void key_press(xcb_generic_event_t *ev)
{
	xcb_key_press_event_t *key_event = (xcb_key_press_event_t *)ev;
	//printf ("Key pressed in window\n");
	if(key_event->child) {
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
	if(Current)
		xcb_change_window_attributes(connection, Current->window, XCB_CW_BORDER_PIXEL, &infocus_color);
	xcb_flush(connection);
	free(ev);
}

void focus_out(xcb_generic_event_t *ev)
{
	xcb_focus_out_event_t *focusout_ev = (xcb_focus_out_event_t *) ev;
	Current = bfs_search(Root, focusout_ev->event);
	if(Current)
		xcb_change_window_attributes(connection, Current->window, XCB_CW_BORDER_PIXEL, &outfocus_color);
	xcb_flush(connection);
	free(ev);
}

void client_message(xcb_generic_event_t *ev)
{
	xcb_client_message_event_t *client_msg = (xcb_client_message_event_t *) ev;
	printf("client message, window %d ", client_msg->window);
	
	if(client_msg->type == netatom[NetWMState])
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
	if(xcb_connection_has_error(connection)) {
		fprintf(stderr, "tdwm: could not connect to X server, exiting\n");
		exit(1);
	}
	screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
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
	for(int i=0; i<SETUP_NUM_ATOMS; i++) {
		atom_cookie[i] = xcb_intern_atom(connection, 0, strlen(setup_atoms[i].name), setup_atoms[i].name);
	}
	for(int i=0; i<SETUP_NUM_ATOMS; i++) {
		xcb_intern_atom_reply_t *reply;
		if((reply = xcb_intern_atom_reply(connection, atom_cookie[i], NULL))) {
			if(setup_atoms[i].isnet)
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
	 		XCB_EVENT_MASK_ENTER_WINDOW | 		XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
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

int main(int argc, char **argv)
{
	setup();
	for(;;) {
		ev = xcb_wait_for_event(connection);
		if(handler[ev->response_type & ~0x80])
			handler[ev->response_type & ~0x80](ev);
		else free(ev);
	}
	return 0;
}
