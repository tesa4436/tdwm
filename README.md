# tdwm

## Introduction

This is my attempt to write a tiling window manager for Xorg. It started out as a summer project, and it is yet to be finished.```tdwm``` uses a binary tree data structure to lay windows on the screen, just like i3wm. Furthermore, it splits windows either
horizontally or vertically, splitting the focused window size in half.

## Compiling and running

This is a work-in-progress, so no fancy MakeFile is present yet, just a basic one. It should compile, provided the required library headers (the XCB ones) are present in your system.

I test this with ```Xephyr```, an X server implementation running in a window, instead of the framebuffer. This way there's no need to constantly switch between window managers.
