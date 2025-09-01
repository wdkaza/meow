#pragma once
#include <X11/X.h>
#include <stdint.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

// multi monitors not supported since i have no wqay to debug them
// i realised too late you could probably run multiple xephyr sesions to debug them

//// default keybinds suck TODO make them better later

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

#define START_WINDOW_GAP 20
#define DESKTOP_COUNT 10
#define BORDER_WIDTH 4
#define BORDER_FOCUSED_WIDTH 4 // currently a bit broken visually(layout will look ugly)
#define BORDER_COLOR 0x404231
#define BORDER_FOCUSED_COLOR 0xf2fcb1

#define BG_COLOR 0x000000
#define BAR_REFRESH_TIME 1 // in seconds

// new config 

// ignore
typedef union{
  int i;
  const void *v;
  Window win;
} Arg;
// ignore
void spawn(Arg *arg);
void kill(Arg *arg);
void increaseVolume(Arg *arg);
void decreaseVolume(Arg *arg);
void muteVolume(Arg *arg);
void addWindowToLayout(Arg *arg);
void increaseBrightness(Arg *arg);
void decreaseBrightness(Arg *arg);
void minBrightness(Arg *arg);
void decreaseGapSize(Arg *arg);
void increaseGapSize(Arg *arg);
void setWindowLayoutTiled(Arg *arg);
void setWindowLayoutFloating(Arg *arg);
void moveWindowUp(Arg *arg);
void moveWindowDown(Arg *arg);
void fullscreen(Arg *arg);
void disableBar(Arg *arg);
void enableBar(Arg *arg);
void cycleWindows(Arg *arg);
void switchDesktop(Arg *arg);
void transferWindowToDesktop(Arg *arg);
// ignore
typedef struct KeyEvent {
  unsigned int modifier;
  KeySym key;
  void(*func)(Arg *a);
  Arg arg;
} KeyEvent;


// all actions you can find below
//
// spawn,        (value required), (pretty much executes commands so can be used for them too like adjusting volume)
// kill,
// addToLayout
// moveWindowUp  (inLayout)
// moveWindowDown(inLayout)
// disableBar     
// enableBar     
// increaseVolume
// decreaseVolume
// muteVolume
// increaseBrightness
// decreaseBrigthness
// minBrigthness (1%)
// increaseGapSize
// decreaseGapSize
// cycleWindows
// fullscreen
// setWindowLayoutTiled
//

#define MOD Mod1Mask // alt by default

// ===define values HERE ===
// keepsame |  [name]   |   [args]
static char *terminal[] = {"kitty", NULL};
static char *launcher[] = {"rofi", "-show", NULL};
// ===define values HERE ===


// [MOD-KEY, KEY, ACTION, VALUE(if none set to 0)]
// [MOD-KEY|ShiftMask, ...]   for mod+shift keybinds
// [MOD-KEY|ControlMask, ...] for mod+control keybinds

struct KeyEvent keys[] = {
  //
  // {MOD, XK_F3,  spawn,              {.v = (const char *[]){"pactl", "set-sink-volume", "@DEFAULT_SINK", "+5%", NULL}}}
  // split args by coma and double quote them "arg"
  // ^^^ example of custom command ^^^
  // keeping it simple by using increaseVolume/decreaseVolume/muteVolume, etc
  {MOD, XK_Return, spawn,                   {.v = terminal}},
  {MOD, XK_slash,  spawn,                   {.v = launcher}},
  {MOD, XK_q,      kill,                    {0}},
  {MOD, XK_space,  addWindowToLayout,       {0}},
  {MOD, XK_Up,     moveWindowUp,            {0}},
  {MOD, XK_Down,   moveWindowDown,          {0}},
  {MOD, XK_n,      enableBar,               {0}},
  {MOD, XK_m,      disableBar,              {0}},
  {MOD, XK_F3,     increaseVolume,          {0}},
  {MOD, XK_F2,     decreaseVolume,          {0}},
  {MOD, XK_F1,     muteVolume,              {0}},
  {MOD, XK_F6,     increaseBrightness,      {0}},
  {MOD, XK_F5,     decreaseBrightness,      {0}},
  {MOD, XK_F7,     minBrightness,           {0}},
  {MOD, XK_y,      increaseGapSize,         {0}},
  {MOD, XK_u,      decreaseGapSize,         {0}},
  {MOD, XK_Tab,    cycleWindows,            {0}},
  {MOD, XK_f,      fullscreen,              {0}},
  {MOD, XK_r,      setWindowLayoutTiled,    {0}},
  {MOD, XK_t,      setWindowLayoutFloating, {0}},

  // desktop related keybindings
  {MOD, XK_1,      switchDesktop,           {.i = 1}},
  {MOD, XK_2,      switchDesktop,           {.i = 2}},
  {MOD, XK_3,      switchDesktop,           {.i = 3}},
  {MOD, XK_4,      switchDesktop,           {.i = 4}},
  {MOD, XK_5,      switchDesktop,           {.i = 5}},
  {MOD, XK_6,      switchDesktop,           {.i = 6}},
  {MOD, XK_7,      switchDesktop,           {.i = 7}},
  {MOD, XK_8,      switchDesktop,           {.i = 8}},
  {MOD, XK_9,      switchDesktop,           {.i = 9}},

  {MOD|ShiftMask, XK_1, transferWindowToDesktop, {.i = 1}},
  {MOD|ShiftMask, XK_2, transferWindowToDesktop, {.i = 2}},
  {MOD|ShiftMask, XK_3, transferWindowToDesktop, {.i = 3}},
  {MOD|ShiftMask, XK_4, transferWindowToDesktop, {.i = 4}},
  {MOD|ShiftMask, XK_5, transferWindowToDesktop, {.i = 5}},
  {MOD|ShiftMask, XK_6, transferWindowToDesktop, {.i = 6}},
  {MOD|ShiftMask, XK_7, transferWindowToDesktop, {.i = 7}},
  {MOD|ShiftMask, XK_8, transferWindowToDesktop, {.i = 8}},
  {MOD|ShiftMask, XK_9, transferWindowToDesktop, {.i = 9}},
};

// ignore
typedef enum {
  SEGMENT_LEFT = 0,
  SEGMENT_CENTER,
  SEGMENT_RIGHT
} SegmentPosition;
// ignore
typedef struct {
  char name[32];
  char command[256];
  char format[64];
  SegmentPosition position;
  bool enabled;
} BarModuleConfig;
// ignore ^^^^^






#define BAR_HEIGHT 24
#define BAR_COLOR 0x282a36
#define BAR_FONT "JetBrains Mono Nerd Font:size11:style=bold"
#define BAR_FONT_SIZE 18
#define BAR_FONT_COLOR "#f8f8f8"
#define BAR_PADDING_X 0
#define BAR_PADDING_Y 0
#define DESKTOP_HIGHLIGHT_COLOR "#f8f8f8" // ueless, keep it the same as BAR_FONT_COLOR if this will not be removed
#define BAR_MODULE_PADDING 10 // padding from the sides of the bar
#define BAR_SEGMENT_SPACING 30 // padding between modules

// format:
// .name = 
// .command = 
// .format = 
// .enabled = (isnt enabled useless?? whatever) 

#define BAR_SEGMENTS_COUNT 5

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
    .enabled = true
  },
  {
    .name = "string", //string too heavy and causes a small bug, will try to fix it later
    .command = "date +%H:%M:%S",
    .format = "%s",
    .position = SEGMENT_CENTER,
    .enabled = false
  },
  {
    .name = "desktop",
    .format = "%s",
    .position = SEGMENT_LEFT,
    .enabled = true
  }
};
