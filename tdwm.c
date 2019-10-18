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

/*
 * cur - current window, passed into the function
 * amt - amount of pixels, can be negative
 * dir - direction, either vertical or horizontal
 * loclen - local_length, used by change_dimensions, it is the length of the
 * to be resized group
 *
 * soe - south or east
 * xoy - x or y
 * won - west or north
 * woh_mask - width or height mask for for the X11 resize function 
 */

void resize_window_group(Window *cur, int32_t amt, unsigned char dir)
{
	uint32_t loclen;
	uint8_t mode, soe, xoy, yox, won;
	uint16_t woh_mask;
	Window *cur2;

	if (!cur)
		return;

	switch (dir) {
		case EAST: {
			mode = HORIZONTAL;
			soe = SOUTH;
			won = WEST;
			xoy = X;
			yox = Y;
			woh_mask = XCB_CONFIG_WINDOW_WIDTH; 

		} break;

		case SOUTH: {
			mode = VERTICAL;
			soe = EAST;
			won = NORTH;
			yox = X;
			xoy = Y;
			woh_mask = XCB_CONFIG_WINDOW_HEIGHT; 
		} break;

		default: return;
	}

	loclen = calc_len(cur->n[dir], NULL, dir);

	if (loclen) { 
		cur->dim[xoy + 2] += amt;
		xcb_configure_window(	con,
					cur->wnd,
					woh_mask,
					cur->dim + xoy + 2);

		change_dimensions(	cur->n[dir],
					cur->n[dir]->dim[xoy] + amt,
					amt * -1,
					cur->n[dir],
					loclen,
					mode);
		xcb_flush(con);

		cur2 = cur->n[dir];
	
		while (cur2) {
			if (cur2->dim[yox] +
				cur2->dim[yox + 2] >
				cur->dim[yox] + cur->dim[yox + 2])
			{
				loclen = calc_len(cur->n[soe], NULL, dir);
			
				if (loclen) {
					change_dimensions(cur->n[soe],
							cur->n[soe]->dim[xoy],
							amt,
							cur->n[soe],
							loclen,
							mode);
					xcb_flush(con);
				}
				break;
			}
			cur2 = cur2->n[soe];
		}
	} else {
		cur = find_neighboring_group(cur, mode);

		if (!cur)
			return;

		loclen = calc_len(cur, NULL, dir);
		change_dimensions(cur,
				cur->dim[xoy] + amt,
				amt * -1,
				cur,
				loclen,
				mode);

		loclen = 0;
		
		while ((cur = cur->n[won])) {
			cur2 = cur;
			loclen += cur->dim[xoy + 2];
		}
		
		change_dimensions(cur2,
				cur2->dim[xoy],
				amt,
				cur2,
				loclen,
				mode);
		xcb_flush(con);
	}
}

Window *find_neighboring_group(Window *cur, const unsigned char flag)
{
	Window *cur2 = cur;
	uint32_t other_grp_dim;
	uint8_t eos, won, now, xoy;

	if (!cur)
		return NULL;

	switch (flag) {
		case HORIZONTAL: {
			eos = EAST;
			won = WEST;
			now = NORTH;
			xoy = X;
		} break;

		case VERTICAL: {
			eos = SOUTH;
			won = NORTH;
			now = WEST;
			xoy = Y;
		} break;

		default: return NULL;
	}

	if (cur->n[eos] && (cur2 = cur->n[eos]))
		while (cur2->n[eos])
			cur2 = cur2->n[eos];

	other_grp_dim = cur2->dim[xoy] +
			cur2->dim[xoy + 2] + BORDER_WIDTH*2;

	while (cur->n[won])
		cur = cur->n[won];

	for (cur = cur->n[now]; cur; cur = cur->n[now]) {
		cur2 = cur->n[eos];

		while (cur2) {
			if (cur2->dim[xoy] == other_grp_dim)
				return cur2;

			cur2 = cur2->n[eos];
		}

		while (cur->n[won])
			cur = cur->n[won];
	}

	return NULL;
}

uint32_t calc_len(Window *cur, const Window *limwnd, const unsigned char flag)
{
	if (!cur)
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

	Window *cur2;
	sum += cur->dim[off1] + BORDER_WIDTH*2;

	while ((cur = cur->n[off2])) {
		if (limwnd) {
			if (cur->dim[off3] > limwnd->dim[off3])
				break;

			if (flag == EAST || flag == SOUTH) {
				cur2 = cur->n[off4];

				while (cur2 &&
					cur2->dim[off3 - 2] +
					cur2->dim[off3] <=
					limwnd->dim[off3 - 2] +
					limwnd->dim[off3])
						cur2 = cur2->n[off4];
// this loop checks whether this window group is bigger than the limit window

				if (cur2)
					break;
			}
		}
		sum += cur->dim[off1] + BORDER_WIDTH*2;
	}
	return sum;
}

void change_dimensions(Window *cur, const uint32_t x, const int32_t w, Window *limwnd, uint32_t lw, const unsigned char flag)
{
	if (!limwnd)
		return;

	struct stack_node stack[4]; // a stack for iterative tree traversal
	size_t wndc = 0;
	uint32_t rem = 0, oldw = 0, wsm = 0, nlw;
	int64_t rsm = 0, oldrsm = 0;
	unsigned char xoy, how, won, now, eos, skip;
	uint16_t xoy_mask, woh_mask;

	if (flag == HORIZONTAL) {
		xoy = X;
		how = HEIGHT;
		won = WEST;
		now = NORTH;
		eos = EAST;
		xoy_mask = XCB_CONFIG_WINDOW_X;
		woh_mask = XCB_CONFIG_WINDOW_WIDTH;
	} else if (flag == VERTICAL) {
		xoy = Y;
		how = WIDTH;
		won = NORTH;
		now = WEST;
		eos = SOUTH;
		xoy_mask = XCB_CONFIG_WINDOW_Y;
		woh_mask = XCB_CONFIG_WINDOW_HEIGHT;
	} else return;

	if (limwnd->dim[how - 2] +
		limwnd->dim[how] + (BORDER_WIDTH * 2) <=
		cur->dim[how - 2])
			return;

	wndc++;
	stack[0].wnd = cur;
	stack[0].rsm = 0;
	stack[0].wsm = 0;

	if (cur != limwnd) {
		stack[0].skip = 0;
		stack[0].lw = lw;
		stack[0].nlw = lw + (w * -1);
	} else if ((w < 0 && (uint32_t) (w * -1) < cur->dim[xoy + 2]) ||
			(w >= 0 && (uint32_t) w < lw))
	{
		stack[0].skip = 1;
		stack[0].lw = lw;
		stack[0].nlw = lw + (w * -1);
	} else return;

	while (wndc) {
		cur = stack[wndc - 1].wnd;
		rsm = stack[wndc - 1].rsm;
		wsm = stack[wndc - 1].wsm;
		skip = stack[wndc - 1].skip;
		lw = stack[wndc - 1].lw;
		nlw = stack[wndc - 1].nlw;

		wsm += cur->dim[xoy + 2];
		rem = (cur->dim[xoy + 2] * lw) % nlw; 
		rsm += rem;
		oldrsm = rsm;
		oldw = cur->dim[xoy + 2];
		cur->dim[xoy + 2] =
		(cur->dim[xoy + 2] * lw) / nlw;

		wndc--;

		if (cur != limwnd) {
			if (cur->n[won])
				cur->dim[xoy] =
				cur->n[won]->dim[xoy] +
				cur->n[won]->dim[xoy + 2] + BORDER_WIDTH*2;
			else if (cur->n[now])
				cur->dim[xoy] =
				cur->n[now]->dim[xoy];
			else
				cur->dim[xoy] = x;
		} else cur->dim[xoy] = x;

		//printf("shadman %u %ld %u %u\n", cur->window, rsm, rem, (wsm * lw) % nlw);
		//
		if (cur->n[EAST]) {
			if (flag == HORIZONTAL) {
				if (cur->n[EAST]->dim[X] <
					limwnd->dim[X] + lw)
				{
					wndc++;
					stack[wndc - 1].rsm = rsm;
					stack[wndc - 1].wsm = wsm;
					stack[wndc - 1].lw = lw;
					stack[wndc - 1].nlw = nlw;
					stack[wndc - 1].wnd = cur->n[EAST];
					stack[wndc - 1].skip = skip;
				} else
					cur->dim[xoy + 2] += rsm / nlw;
			} else if (flag == VERTICAL) {
				if (skip || limwnd->dim[X] +
				limwnd->dim[WIDTH] + (BORDER_WIDTH * 2) >
				cur->n[EAST]->dim[X])
				{
					wndc++;
					stack[wndc - 1].rsm = oldrsm - rem;
					stack[wndc - 1].wsm = wsm - oldw;
					stack[wndc - 1].wnd = cur->n[EAST];
					stack[wndc - 1].lw = lw;
					stack[wndc - 1].nlw = nlw;
					stack[wndc - 1].skip = skip;
				}
			}
		} else if (flag == HORIZONTAL)
			cur->dim[xoy + 2] += rsm / nlw;

		if (cur->n[SOUTH]) {
			if (flag == HORIZONTAL) {
				if (skip || limwnd->dim[Y] +
				limwnd->dim[HEIGHT] + (BORDER_WIDTH * 2) >
				cur->n[SOUTH]->dim[Y])
				{
					wndc++;
					stack[wndc - 1].rsm = oldrsm - rem;
					stack[wndc - 1].wsm = wsm - oldw;
					stack[wndc - 1].lw = lw;
					stack[wndc - 1].nlw = nlw;
					stack[wndc - 1].wnd = cur->n[SOUTH];
					stack[wndc - 1].skip = skip;
				}
			} else if (flag == VERTICAL) {
				if (cur->n[SOUTH]->dim[Y] <
				limwnd->dim[Y] + lw)
				{
					wndc++;
					stack[wndc - 1].rsm = rsm;
					stack[wndc - 1].wsm = wsm;
					stack[wndc - 1].lw = lw;
					stack[wndc - 1].nlw = nlw;
					stack[wndc - 1].wnd = cur->n[SOUTH];
					stack[wndc - 1].skip = skip;
				} else
					cur->dim[xoy + 2] += rsm / nlw;
			}
		} else if (flag == VERTICAL)
			cur->dim[xoy + 2] += rsm / nlw;

		xcb_configure_window(con,
					cur->wnd,
					xoy_mask,
					cur->dim + xoy);
		xcb_configure_window(con,
					cur->wnd,
					woh_mask,
					cur->dim + xoy + 2);
	}
}


void insert_window_after(Window *tree_root, const xcb_window_t after_which, const xcb_window_t new_window) // this function uses a bunch of global variables, TODO make local
{
	Window Temp;
	unsigned char soe, now, how, woh;
	uint16_t mask2;
	xcb_get_geometry_cookie_t root_geom_cookie;

	if (after_which == root) {
		root_geom_cookie = xcb_get_geometry(con, root);
		Root = malloc(sizeof(Window));

		if (!Root) {
			fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
			exit(1);
		}

		Root->wnd = new_window;

		for (int i = 0; i < 4; i++)
			Root->n[i] = NULL;

		Cur = Root; // Cur is global, probably should be changed to local
		geom = xcb_get_geometry_reply(con, root_geom_cookie, NULL);
		Root->dim[WIDTH] = geom->width - BORDER_WIDTH*2;
		Root->dim[HEIGHT] = geom->height - BORDER_WIDTH*2;
		Root->dim[GROUPWIDTH] = Root->dim[WIDTH];
		Root->dim[GROUPHEIGHT] = Root->dim[HEIGHT];
		Root->dim[X] = 0;
		Root->dim[Y] = 0;
		xcb_configure_window(con, Root->wnd,	XCB_CONFIG_WINDOW_X |
								XCB_CONFIG_WINDOW_Y |
								XCB_CONFIG_WINDOW_WIDTH |
								XCB_CONFIG_WINDOW_HEIGHT,
								Root->dim);
	} else {
		Cur = bfs_search(tree_root, after_which);

		if (!Cur)
			return;
		Temp = *Cur;
		Prev = Cur;
		Window *arr[4];

		if (split_mode == 'v') {
			soe = SOUTH;
			now = NORTH;
			how = HEIGHT;
			woh = WIDTH;
			mask2 = XCB_CONFIG_WINDOW_HEIGHT;
			arr[EAST] = NULL;
			arr[SOUTH] = Temp.n[soe];
			arr[NORTH] = Prev;
			arr[WEST] = NULL;
		} else {
			soe= EAST;
			now = WEST;
			how = WIDTH;
			woh = HEIGHT;
			mask2 = XCB_CONFIG_WINDOW_WIDTH;
			arr[EAST] = Temp.n[soe];
			arr[SOUTH] = NULL;
			arr[NORTH] = NULL;
			arr[WEST] = Prev;
		}

		Cur->n[soe] = malloc(sizeof(Window));

		if (!Cur->n[soe]) {
			fprintf(stderr, "tdwm: error: could not allocate %lu bytes\n", sizeof(Window));
			exit(2);
		}

		Cur->n[soe]->wnd = new_window;

		for (int i = 0; i < 4; i++)
			Cur->n[soe]->n[i] = arr[i];

		if (Cur->n[soe]->n[soe])
			Cur->n[soe]->n[soe]->n[now] =
			Cur->n[soe];

		Cur->n[soe]->dim[how] =
		Cur->dim[how] % 2;

		Cur->dim[how] =
		(Cur->dim[how] + BORDER_WIDTH * 2) / 2 - BORDER_WIDTH * 2;

		Cur->n[soe]->dim[now - 2] =
		Cur->dim[now - 2]; // either east or south neighbour's x or y

		Cur->n[soe]->dim[how - 2] =
		Cur->dim[how - 2] +
		((Cur->dim[how] + BORDER_WIDTH * 2)); // y or x

		Cur->n[soe]->dim[how] +=
		Cur->dim[how];

		Cur->n[soe]->dim[woh] =
		Cur->dim[woh];

		Cur->n[soe]->dim[GROUPWIDTH] =
		Cur->n[soe]->dim[WIDTH];

		Cur->n[soe]->dim[GROUPHEIGHT] =
		Cur->n[soe]->dim[HEIGHT];

		if (Cur->n[soe]->n[soe]) {
			Cur->n[soe]->dim[how + 2] +=
			Cur->n[soe]->n[soe]->
			dim[how + 2]; // if new window is in between other two, add the following
							//	group w
		}

		xcb_configure_window(con,
					Cur->wnd,
					mask2,
					Cur->dim + how);
		xcb_configure_window(con,
					Cur->n[soe]->wnd,
					XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
					XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
					Cur->n[soe]->dim);
	}
}

Window* bfs_search(Window *cur, const xcb_window_t key)
{
	if (!cur)
		return NULL;
	Window *east = NULL, *south = NULL;
	if (cur->wnd == key)
		return cur;
	if (cur->n[EAST])
		east = bfs_search(cur->n[EAST], key);
	if (east)
		return east;
	if (cur->n[SOUTH])
		south = bfs_search(cur->n[SOUTH], key);
	return south;
}

void print_node(Window *root, xcb_window_t win)
{
	Window *window = bfs_search(root, win);
	Window *other_group = find_neighboring_group(window, HORIZONTAL);

	if (window) {
		if (window == Root)
			printf("----ROOT----\n");
		printf("node %d\n",window->wnd);
		
		if (window->n[NORTH])
			printf("north %d ", window->n[NORTH]->wnd);
		if (window->n[EAST])
			printf("east %d ", window->n[EAST]->wnd);
		if (window->n[SOUTH])
			printf("south %d ", window->n[SOUTH]->wnd);
		if (window->n[WEST])
			printf("west %d\n", window->n[WEST]->wnd);
		printf("\n%u %u %u %u\n\n", window->dim[X], window->dim[Y], window->dim[WIDTH], window->dim[HEIGHT]);

		if (other_group) {
			printf("other group %u\n", other_group->wnd);
		} else
			printf("other group null\n");
	}
}

void map_request(xcb_generic_event_t *ev)
{
	xcb_map_request_event_t *mapreq_ev = (xcb_map_request_event_t *) ev;
	xcb_get_property_cookie_t wm_hints_cookie =
		xcb_icccm_get_wm_hints(con, mapreq_ev->window);

	xcb_window_t prop = 0;
	values[0] = BORDER_WIDTH;
	geom = NULL;
	xcb_configure_window(con,
				mapreq_ev->window,
				XCB_CONFIG_WINDOW_BORDER_WIDTH,
				values);

	if (xcb_icccm_get_wm_transient_for_reply(con,
						xcb_icccm_get_wm_transient_for(con, mapreq_ev->window),
						&prop, NULL))
	{
		Cur = bfs_search(Root, prop);
		geom = xcb_get_geometry_reply(con,
		xcb_get_geometry(con, mapreq_ev->window), NULL);

		if ((Cur->dim[WIDTH] > geom->width) &&
			(Cur->dim[HEIGHT] > geom->height))
		{
			values[0] = Cur->dim[X] +
			Cur->dim[WIDTH]/2 - geom->width/2;

			values[1] = Cur->dim[Y] +
			Cur->dim[HEIGHT]/2 - geom->height/2;
		} else {
			values[0] = Root->dim[WIDTH]/2;
			values[1] = Root->dim[HEIGHT]/2;
		}
		xcb_configure_window(con,
					mapreq_ev->window,
					XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
					values);
	} else
		insert_window_after(Root, focused_window, mapreq_ev->window); // insert mapreq_ev->window after focused_window
	if (xcb_icccm_get_wm_hints_reply(con, wm_hints_cookie, &wm_hints, NULL)) {
		xcb_icccm_wm_hints_set_normal(&wm_hints);
		xcb_icccm_set_wm_hints(con, mapreq_ev->window, &wm_hints);
	}

	values[0] = XCB_EVENT_MASK_ENTER_WINDOW |
			XCB_EVENT_MASK_EXPOSURE |
			XCB_EVENT_MASK_FOCUS_CHANGE;

	xcb_change_window_attributes(con,
					mapreq_ev->window,
					XCB_CW_EVENT_MASK,
					values);			

	xcb_map_window(con,mapreq_ev->window);
	focused_window = mapreq_ev->window;
	xcb_set_input_focus(con,
				XCB_INPUT_FOCUS_POINTER_ROOT,
				mapreq_ev->window,
				XCB_CURRENT_TIME);

	xcb_change_property(con,
				XCB_PROP_MODE_REPLACE,
				root,
				netatom[NetActiveWindow],
				XCB_WINDOW,
				32,
				1,
				&mapreq_ev->window);
	xcb_flush(con);

	free(geom);
	geom = NULL;
	free(ev);
}

void unmap_notify(xcb_generic_event_t *ev)
{
	char dir_east;
	uint32_t lw, local_height, lwoh;
	unsigned char off1, off2, off3, off4, off5, off6, off7;
	Window *Temp1 = NULL, *Temp2 = NULL;
	xcb_unmap_notify_event_t *unmap_ev = (xcb_unmap_notify_event_t *) ev; 
	xcb_get_property_cookie_t wm_hints_cookie = xcb_icccm_get_wm_hints(con, unmap_ev->window);

	Cur = bfs_search(Root, unmap_ev->window);
	
	if (!Cur)
		return;

	if (Cur->n[EAST] && Cur->n[SOUTH]) {
		dir_east = (Cur->dim[HEIGHT] + BORDER_WIDTH*2) <
		calc_len(Cur->n[EAST], NULL, SOUTH) ? 0 : 1;

		if (dir_east) {
			off1 = EAST;
			off2 = SOUTH;
			off3 = NORTH;
			off4 = WEST;
			off5 = HORIZONTAL;
			off6 = X;
			off7 = HEIGHT;
		} else {
			off1 = SOUTH;
			off2 = EAST;
			off3 = WEST;
			off4 = NORTH;
			off5 = VERTICAL;
			off6 = Y;
			off7 = WIDTH;
		}

		lwoh = calc_len(Cur, Cur, off1);
		Cur2 = Cur->n[off1];

		while (Cur2->n[off2])
			Cur2 = Cur2->n[off2];	

		Cur2->n[off2] = Cur->n[off2];
		Cur->n[off2]->n[off3] = Cur2;
		Cur2 = Cur->n[off1];

		if (Cur->n[off3]) {
			Cur->n[off1]->n[off3] = Cur->n[off3];
			Cur->n[off3]->n[off2] = Cur->n[off1];
		} else
			Cur->n[off1]->n[off3] = NULL;

		if (Cur->n[off4]) {
			Cur->n[off1]->n[off4] = Cur->n[off4];
			Cur->n[off4]->n[off1] = Cur->n[off1];
		} else
			Cur->n[off1]->n[off4] = NULL;

		change_dimensions(Cur->n[off1],
					Cur->dim[off6],
					Cur->dim[off6 + 2],
					Cur,
					lwoh,
					off5);
		xcb_flush(con);
		// a rather awkward place for a resize function call, TODO restructure

		if (Cur == Root) {
			Root = dir_east ?
				Cur->n[EAST] :
				Cur->n[SOUTH];
			Root->n[NORTH] = NULL;
			Root->n[WEST] = NULL;
		}

		free(Cur);
		Cur = NULL;

		if (xcb_icccm_get_wm_hints_reply(con, wm_hints_cookie, &wm_hints, NULL)) {
			xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
			xcb_icccm_set_wm_hints(con, unmap_ev->window, &wm_hints);
		}

		free(ev);
		return;
	} else if (!Cur->n[EAST] && Cur->n[SOUTH]) {
		off1 = SOUTH;
		off2 = WEST;
		off3 = NORTH;
		off4 = EAST;
		off5 = VERTICAL;
		off6 = WIDTH;
		off7 = Y;
	} else if (Cur->n[EAST] && !Cur->n[SOUTH]) {
		off1 = EAST;
		off2 = NORTH;
		off3 = WEST;
		off4 = SOUTH;
		off5 = HORIZONTAL;
		off6 = HEIGHT;
		off7 = X;
	} else {
		Cur2 = Cur;

		if (Cur->n[NORTH]) {
			off1 = NORTH;
			off2 = SOUTH;
			off3 = WIDTH;
			off4 = VERTICAL;
			off5 = Y;
		} else if (Cur->n[WEST]) {
			off1 = WEST;
			off2 = EAST;
			off3 = HEIGHT;
			off4 = HORIZONTAL;
			off5 = X;
		} else {
			free(Cur);
			Cur = NULL;
			Root = NULL;
			if (xcb_icccm_get_wm_hints_reply(con, wm_hints_cookie, &wm_hints, NULL)) {
				xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
				xcb_icccm_set_wm_hints(con, unmap_ev->window, &wm_hints);
			}
			free(ev);
			return;
		}
		lwoh = calc_len(Cur, Cur, off1); // TODO maybe move the length calculations to the resize function?
		Cur->n[off1]->n[off2] = NULL;

		while (Cur2->n[off1] &&
			Cur2->n[off1]->dim[off3] <=
			Cur->dim[off3])
				Cur2 = Cur2->n[off1];

		change_dimensions(Cur2,
					Cur2->dim[off5],
					Cur->dim[off5 + 2],
					Cur,
					lwoh,
					off4);
		xcb_flush(con);
		free(Cur);
		Cur = NULL;

		if (xcb_icccm_get_wm_hints_reply(con, wm_hints_cookie, &wm_hints, NULL)) {
			xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
			xcb_icccm_set_wm_hints(con, unmap_ev->window, &wm_hints);
		}
		free(ev);
		return;
	}
	lwoh = calc_len(Cur, Cur, off1); // TODO these 3 length calculations waste resources, put them in separate scopes, refactoring required
	Cur2 = Cur;
	Temp1 = Cur->n[EAST];
	Temp2 = Cur->n[SOUTH];
	Cur->n[EAST] = NULL;
	Cur->n[SOUTH] = NULL;

	if (Cur->n[NORTH])
		local_height = calc_len(Cur, Cur, NORTH); // or perhaps precalculate the lengths and store them in the nodes?..
	else if (Cur->n[WEST])
		lw = calc_len(Cur, Cur, WEST);

	Cur->n[EAST] = Temp1;
	Cur->n[SOUTH] = Temp2;

	if (Cur == Root) {
		Root = Cur->n[off1];
	Root->n[NORTH] = NULL;
		Root->n[WEST] = NULL;
	} else if (Cur->n[off2]) {	
		Cur->n[off2]->n[off4] = Cur->n[off1];
		Cur->n[off1]->n[off3] = NULL;
		Cur->n[off1]->n[off2] = Cur->n[off2];
	} else {
		Cur->n[off3]->n[off1] = Cur->n[off1];
		Cur->n[off1]->n[off2] = NULL;
		Cur->n[off1]->n[off3] = Cur->n[off3];
	}

	if ((Cur->dim[off6] + BORDER_WIDTH*2) >=
		calc_len(Cur->n[off1], NULL, off4))
			change_dimensions(Cur->n[off1],
						Cur->dim[off7],
						Cur->dim[off7 + 2],
						Cur,
						lwoh,
						off5);
	else if (Cur->n[NORTH]) {
		Window *Temp = Cur->n[NORTH]->n[SOUTH];
		Cur->n[NORTH]->n[SOUTH] = NULL;

		while (Cur2->n[NORTH] &&
			Cur2->n[NORTH]->dim[WIDTH] <=
			Cur->dim[WIDTH])
		{
			Cur2 = Cur2->n[NORTH];
		}

		change_dimensions(Cur2,
					Cur2->dim[Y],
					Cur->dim[HEIGHT],
					Cur,
					local_height,
					VERTICAL);

		Cur->n[NORTH]->n[SOUTH] = Temp;
	} else if (Cur->n[WEST]) {
		Window *Temp = Cur->n[WEST]->n[EAST];
		Cur->n[WEST]->n[EAST] = NULL;

		while (Cur2->n[WEST] &&
			Cur2->n[WEST]->dim[HEIGHT] <=
			Cur->dim[HEIGHT])
		{
			Cur2 = Cur2->n[WEST];
		}

		change_dimensions(Cur2,
					Cur2->dim[X],
					Cur->dim[WIDTH],
					Cur,
					lw,
					HORIZONTAL);

		Cur->n[WEST]->n[EAST] = Temp;
	}

	xcb_flush(con);
	free(Cur);
	Cur = NULL;

	if (xcb_icccm_get_wm_hints_reply(con, wm_hints_cookie, &wm_hints, NULL)) {
		xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
		xcb_icccm_set_wm_hints(con, unmap_ev->window, &wm_hints);
	}
	
	free(ev);
}

struct key_mapping* get_keyboard_mapping(xcb_connection_t *con, const xcb_setup_t *setup)
{
	xcb_get_keyboard_mapping_reply_t *keyboard_mapping =
	xcb_get_keyboard_mapping_reply(con, xcb_get_keyboard_mapping(
								con,
								setup->min_keycode,
															setup->max_keycode - setup->min_keycode + 1),
												NULL);
	int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;
	int nkeysyms = keyboard_mapping->length;
	xcb_keysym_t *keysyms = (xcb_keysym_t*) (keyboard_mapping + 1);
	struct key_mapping *ret = malloc(sizeof(struct key_mapping)); // it  isn't necessary to free this, since it is required throughout the running time

	if (!ret) {
		free(keyboard_mapping);
		fprintf(stderr, "error, could not allocate %lu bytes\n", sizeof(struct key_mapping));
		exit(1);
	}

	ret->keysyms = keysyms;
	ret->no_of_keycodes = nkeycodes;
	ret->no_of_keysyms = nkeysyms;
	ret->keysyms_per_keycode = keyboard_mapping->keysyms_per_keycode;
	printf("nkeycodes %u  nkeysyms %u  keysyms_per_keycode %u\n\n", nkeycodes, nkeysyms, keyboard_mapping->keysyms_per_keycode);

	for(int keycode_idx = 0; keycode_idx < nkeycodes; keycode_idx++) {
		printf("keycode %3u ", setup->min_keycode + keycode_idx);
			for (int keysym_idx = 0; keysym_idx < keyboard_mapping->keysyms_per_keycode; keysym_idx++) {
				printf(" %c", keysyms[keysym_idx + keycode_idx * keyboard_mapping->keysyms_per_keycode]);
			}
		putchar('\n');
	}
	return ret;
}

void key_press(xcb_generic_event_t *ev)
{
	xcb_key_press_event_t *key_event = (xcb_key_press_event_t *)ev;

	if (key_event->child) {
		win = key_event->child;
		geom = xcb_get_geometry_reply(con, xcb_get_geometry(con, win), NULL);
		printf("%c pressed\n", kmapping->keysyms[key_event->detail * kmapping->keysyms_per_keycode]);

		Cur = bfs_search(Root, win);
		
		switch (key_event->detail) {
			case 111: resize_window_group(Cur, -10, SOUTH); break;
			//up arrow key keycode
			case 113: resize_window_group(Cur, -10, EAST); break;
			//left arrow key keycode
			case 114: resize_window_group(Cur, 10, EAST); break;
			//right arrow key keycode
			case 116: resize_window_group(Cur, 10, SOUTH); break;
			//down arrow key keycode
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
	Cur = bfs_search(Root, enternotf_ev->event);
	focused_window = enternotf_ev->event;
	xcb_set_input_focus(con, XCB_INPUT_FOCUS_POINTER_ROOT, enternotf_ev->event, XCB_CURRENT_TIME);
	xcb_change_property(con, XCB_PROP_MODE_REPLACE, root, netatom[NetActiveWindow], XCB_WINDOW, 32, 1, &enternotf_ev->event);
	xcb_flush(con);
	free(ev);
}

void focus_in(xcb_generic_event_t *ev)
{
	xcb_focus_in_event_t *focusin_ev = (xcb_focus_in_event_t *) ev;
	focused_window = focusin_ev->event;
	Cur = bfs_search(Root, focusin_ev->event);
	if (Cur)
		xcb_change_window_attributes(con, Cur->wnd, XCB_CW_BORDER_PIXEL, &infocus_color);
	xcb_flush(con);
	free(ev);
}

void focus_out(xcb_generic_event_t *ev)
{
	xcb_focus_out_event_t *focusout_ev = (xcb_focus_out_event_t *) ev;
	Cur = bfs_search(Root, focusout_ev->event);
	if (Cur)
		xcb_change_window_attributes(con, Cur->wnd, XCB_CW_BORDER_PIXEL, &outfocus_color);
	xcb_flush(con);
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
	xcb_alloc_color_reply_t *color_rep = xcb_alloc_color_reply(con, xcb_alloc_color(con, screen->default_colormap, r, g, b), NULL);
	uint32_t color = color_rep->pixel;
	free(color_rep);
	return color;
}

void setup(void)
{
	con = xcb_connect(NULL, NULL);

	if (xcb_connection_has_error(con)) {
		fprintf(stderr, "tdwm: could not connect to X server, exiting\n");
		exit(1);
	}

	xorg_setup = xcb_get_setup(con);
	kmapping = get_keyboard_mapping(con, xorg_setup);
	screen = xcb_setup_roots_iterator(xorg_setup).data;
	root = screen->root;
	focused_window = root;
	gc = xcb_generate_id(con);
	values[0] = 1;
	values[1] = XCB_LINE_STYLE_SOLID | XCB_CAP_STYLE_BUTT | XCB_JOIN_STYLE_MITER;
	xcb_create_gc(con, gc, root, XCB_GC_LINE_WIDTH | XCB_GC_LINE_STYLE | XCB_GC_CAP_STYLE | XCB_GC_JOIN_STYLE, values);
	values[0] = screen->white_pixel;
	values[1] = screen->black_pixel;
	xcb_change_gc(con, gc, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, values);
	
	// this code that sets up atoms was taken from dwm's code
	xcb_intern_atom_cookie_t atom_cookie[SETUP_NUM_ATOMS];

	for (int i=0; i<SETUP_NUM_ATOMS; i++) {
		atom_cookie[i] = xcb_intern_atom(con, 0, strlen(setup_atoms[i].name), setup_atoms[i].name);
	}

	for (int i=0; i<SETUP_NUM_ATOMS; i++) {
		xcb_intern_atom_reply_t *reply;

		if ((reply = xcb_intern_atom_reply(con, atom_cookie[i], NULL))) {
			if (setup_atoms[i].isnet)
				netatom[setup_atoms[i].number] = reply->atom;
			else
				wmatom[setup_atoms[i].number] = reply->atom;
			free(reply);
		}
	}
	infocus_color = get_color((uint16_t) -1, 0, 0);
	outfocus_color = get_color(10000, 10000, 10000);
	xcb_change_property(con, XCB_PROP_MODE_REPLACE, root, netatom[NetSupported], XCB_ATOM, 32, NetLast, (unsigned char*)netatom);
	values[0] = 	XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | 	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
	 		XCB_EVENT_MASK_ENTER_WINDOW | 		XCB_EVENT_MASK_LEAVE_WINDOW |
			XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	xcb_change_window_attributes(con, root, XCB_CW_EVENT_MASK, values);
	xcb_grab_key(con, 0, root, XCB_MOD_MASK_1, 116, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(con, 0, root, XCB_MOD_MASK_1, 111, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(con, 0, root, XCB_MOD_MASK_1, 113, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(con, 0, root, XCB_MOD_MASK_1, 114, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(con, 0, root, XCB_MOD_MASK_1, 55, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC); // letter v
	xcb_grab_key(con, 0, root, XCB_MOD_MASK_1, 43, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC); // letter h
	//xcb_grab_pointer(con, 1, root, XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
	
    //xcb_grab_button(con, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, XCB_MOD_MASK_1);
    //xcb_grab_button(con, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 3, XCB_MOD_MASK_1);
	xcb_flush(con);
}

int main(void)
{
	setup();

	for (;;) {
		ev = xcb_wait_for_event(con);

		if (handler[ev->response_type & ~0x80])
			handler[ev->response_type & ~0x80](ev);
		else free(ev);
	}

	return 0;
}

