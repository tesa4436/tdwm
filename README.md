# tdwm

## Introduction

This is my attempt to write a tiling window manager for Xorg. It started out as a summer project, and it is yet to be finished. The current
code was written in a very ad-hoc manner, so it requires quite a drastic refactoring, since now it looks quite ugly. I was aiming at getting
this code to work, without focusing on structure.
I should mention that this is my very first personal project which isn't a university assignment, so the code probably reeks of
rookie mistakes, poor structuring and so on. I use C and XCB library for this project, a memory-leak test
with ```valgrind``` for now is negative.
I happen to have a certain passion for Linux, the C language, low-level stuff,
which is why I chose to do this, although reading Xorg protocol documentation and
looking up XCB functions isn't much fun in the beginning, but it was quite rewarding to see this finally working.
However, I learned a thing or two in the process. Examples would be
the client/server design, asynchronous code (since XCB is an asynchronous library), general API design, etc. 
```tdwm``` uses a binary tree data structure to lay windows on the screen, just like i3wm. Furthermore, it splits windows either
horizontally or vertically, splitting the focused window size in half. I intend this window manager to be minimalist, that is,
the only thing it does is placing windows in a tiling manner.

## What doesn't work
* For now, window resizing mostly works, but my code's calculations sometimes are off by a few pixels, when the window being splitted
has an odd number size. This issue halted my progress.
* Proper keybindings are not implemented, only hardcoded ones for changing window splitting mode.
* I postponed compliance with ICCCM and EWMH, because it is not high in my priority list, also it's rather tedious to correctly comply with
them.
* The binary tree structure needs some work too, the tree sometimes incorrectly restructures itself after a node deletion.
