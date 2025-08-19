#pragma once
#include <stdint.h>

// multi monitors not supported since i have no wqay to debug them

// default keybinds suck TODO make them better later

#define MONITOR_WIDTH 1920
#define MONITOR_HEIGHT 1080

#define TERMINAL_CMD "kitty &"
#define BROWSER_CMD "firefox &"
#define ROFI_CMD "rofi -show &"

#define VOLUME_UP_CMD "pactl set-sink-volume @DEFAULT_SINK@ +5%"
#define VOLUME_DOWN_CMD "pactl set-sink-volume @DEFAULT_SINK@ -5%"
#define VOLUME_MUTE_CMD "pactl set-sink-mute @DEFAULT_SINK@ toggle"

#define BRIGHTNESS_UP_CMD "brightnessctl set +5%"
#define BRIGHTNESS_DOWN_CMD "brightnessctl set 5%-"
#define BRIGHTNESS_MIN_CMD "brightnessctl set 1%"

#define MASTER_KEY Mod1Mask // alt by default

#define KILL_KEY XK_Q // kill
#define ROFI_KEY XK_slash // "/"
#define TERMINAL_KEY XK_Return
#define BROWSER_KEY XK_W
#define FULLSCREEN_KEY XK_F
#define CYCLE_WINDOWS_KEY XK_Tab

#define VOLUME_UP_KEY XK_F3
#define VOLUME_DOWN_KEY XK_F2
#define VOLUME_MUTE_KEY XK_F1

#define BRIGHTNESS_UP_KEY XK_F6
#define BRIGHTNESS_DOWN_KEY XK_F5
#define BRIGHTNESS_MIN_KEY XK_F7

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
#define BORDER_FOCUSED_WIDTH 4 // currently a bit broken visually(layout will look ugly)
#define BORDER_COLOR 0x404231
#define BORDER_FOCUSED_COLOR 0xf2fcb1
#define BG_COLOR 0x000000

#define SHOW_BATTERY 1

#define BAR_HEIGHT 24
#define BAR_COLOR 0x282a36
#define BAR_FONT "JetBrains Mono Nerd Font:size11:style=bold"
#define BAR_FONT_SIZE 18
#define BAR_FONT_COLOR "#f8f8f8"
#define BAR_PADDING_X 0
#define BAR_PADDING_Y 0
#define DESKTOP_HIGHLIGHT_COLOR "#f8f8f8"
#define BAR_MODULE_PADDING 10 // padding from the sides of the bar
#define BAR_SEGMENT_SPACING 20 // padding between modules

#define BAR_HIDE_KEY XK_M
#define BAR_SHOW_KEY XK_N

#define BAR_REFRESH_TIME 1 // in seconds


#define BAR_SEGMENTS_COUNT 5

typedef enum {
  SEGMENT_LEFT = 0,
  SEGMENT_CENTER,
  SEGMENT_RIGHT
} SegmentPosition;

typedef struct {
  char name[32];
  char command[256];
  char format[64];
  SegmentPosition position;
  bool enabled;
} BarModuleConfig;

// format:
// .name = 
// .command = 
// .format = 
// .enabled = 

static const BarModuleConfig BarSegments[BAR_SEGMENTS_COUNT] = {
  {
    .name = "volume",
    .command = "pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]*%' | head -1 | tr -d '%'",
    .format = "VOL: %d%% |",
    .position = SEGMENT_RIGHT,
    .enabled = true
  },
  {
    .name = "brightness",
    .command = "brightnessctl get | awk -v max=$(brightnessctl max) '{print int($1*100/max)}'",
    .format = "BRT: %d%% |",
    .position = SEGMENT_RIGHT,
    .enabled = true
  },
  {
    .name = "battery",
    .command = "cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo 0",
    .format = "BAT: %d%%",
    .position = SEGMENT_RIGHT,
    .enabled = SHOW_BATTERY
  },
  {
    .name = "time", // broken module
    .command = "date +%H:%M:%S",
    .format = "%s",
    .position = SEGMENT_RIGHT,
    .enabled = true
  },
  {
    .name = "desktop",
    .format = "%s",
    .position = SEGMENT_LEFT,
    .enabled = true
  }
};
