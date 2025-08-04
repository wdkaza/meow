#pragma once
#include <stdint.h>

// multi monitors not supported since i have no wqay to debug them

#define MONITOR_WIDTH 1920
#define MONITOR_HEIGHT 1080

#define TERMINAL_CMD "kitty &"
#define BROWSER_CMD "firefox &"

#define MASTER_KEY Mod1Mask // alt by default

#define KILL_KEY XK_Q // kill window
#define TERMINAL_KEY XK_Return
#define BROWSER_KEY XK_W
#define FULLSCREEN_KEY XK_F
#define CYCLE_WINDOWS_KEY XK_Tab

#define START_WINDOW_GAP 20
#define GAP_INCREASE_KEY XK_U
#define GAP_DECREAS_KEY XK_Y

#define SET_WINDOW_LAYOUT_TILED_MASTER XK_R
#define SET_WINDOW_LAYOUT_FLOATING XK_T 

#define WINDOW_ADD_TO_LAYOUT XK_space
#define WINDOW_LAYOUT_MOVE_UP XK_Down // i might have reversed it so the keys are reversed as well
#define WINDOW_LAYOUT_MOVE_DOWN XK_Up // maybe will fix later but for now this is MOVEUP

#define DESKTOP_COUNT 10
#define DESKTOP_CYCLE_UP XK_Z
#define DESKTOP_CYCLE_DOWN XK_X

#define BORDER_WIDTH 4
#define BORDER_COLOR 0xffb6c1
#define BG_COLOR 0x000000
