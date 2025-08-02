#pragma once
#include <stdint.h>

// multi monitors not supported since i have no wqay to debug them

#define MONITOR_WIDTH 1920
#define MONITOR_HEIGHT 1080

#define TERMINAL_CMD "kitty &"
#define BROWSER_CMD "firefox &"

#define MASTER_KEY Mod1Mask

#define KILL_KEY XK_Q // kill window
#define TERMINAL_KEY XK_Return
#define BROWSER_KEY XK_W
#define FULLSCREEN_KEY XK_F
#define CYCLE_WINDOWS_KEY XK_Tab


#define BORDER_WIDTH 3
#define BORDER_COLOR 0xffb6c1
#define BG_COLOR 0x000000
