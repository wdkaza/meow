#pragma once
#include "structs.h"

#define MONITOR_WIDTH 1920
#define MONITOR_HEIGHT 1080

#define START_WINDOW_GAP 20
#define DESKTOP_COUNT 10
#define BORDER_WIDTH 1
#define BORDER_FOCUSED_WIDTH 1 // currently a bit broken visually(layout will look ugly)
#define BORDER_COLOR 0x1e1e1e
#define BORDER_FOCUSED_COLOR 0xADD8E6

#define BG_COLOR 0x000000
#define BAR_REFRESH_TIME 1 // in seconds

#define MOD Mod1Mask // alt by default

// ===define values HERE ===
// keepsame |  [name]   |   [args] (if space is present seperate by "arg1", "arg2")
static char *terminal[] = {"kitty", NULL};
static char *launcher[] = {"rofi", "-show", NULL};
static char *screenshot[] = {"scrot", NULL};
// ===define values HERE ===


// all actions you can find below
//
// spawn,                 (value required), (pretty much executes commands so can be used for them too like adjusting volume)
// kill,
// addToLayout
// moveWindowUp           (inLayout)
// moveWindowDown         (inLayout)
// disableBar     
// enableBar     
// increaseVolume
// decreaseVolume
// muteVolume
// increaseBrightness     (you can use "spawn" with your custom command if this one wont work, also do the same for others if they do not work for you)
// decreaseBrigthness     
// minBrigthness (1%)
// increaseGapSize
// decreaseGapSize
// cycleWindows
// fullscreen             
// setWindowLayoutTiled
//
// switchDesktop          (value required)
// transferWindowToDesktop(value required)

// [MOD-KEY, KEY, ACTION, VALUE(if none set to 0)]
// [MOD-KEY|ShiftMask, ...]   for mod+shift keybinds
// [MOD-KEY|ControlMask, ...] for mod+control keybinds

struct KeyEvent keys[] = {
  //
  // {MOD, XK_F3,  spawn,              {.v = (const char *[]){"pactl", "set-sink-volume", "@DEFAULT_SINK", "+5%", NULL}}}
  // split args by coma and double quote them "arg"
  // ^^^ example of custom command ^^^
  // keeping it simple by using increaseVolume/decreaseVolume/muteVolume, etc instead of "spawn"
  //
  {MOD, XK_Return, spawn,                   {.v = terminal}},
  {MOD, XK_slash,  spawn,                   {.v = launcher}},
  {MOD, XK_p,      spawn,                   {.v = screenshot}},
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
  // 0 might be bugged visually with the current bar

  {MOD|ShiftMask, XK_1, transferWindowToDesktop, {.i = 1}},
  {MOD|ShiftMask, XK_2, transferWindowToDesktop, {.i = 2}},
  {MOD|ShiftMask, XK_3, transferWindowToDesktop, {.i = 3}},
  {MOD|ShiftMask, XK_4, transferWindowToDesktop, {.i = 4}},
  {MOD|ShiftMask, XK_5, transferWindowToDesktop, {.i = 5}},
  {MOD|ShiftMask, XK_6, transferWindowToDesktop, {.i = 6}},
  {MOD|ShiftMask, XK_7, transferWindowToDesktop, {.i = 7}},
  {MOD|ShiftMask, XK_8, transferWindowToDesktop, {.i = 8}},
  {MOD|ShiftMask, XK_9, transferWindowToDesktop, {.i = 9}},
  // 0 might be bugged visually with the current bar
};




#define BAR_HEIGHT 30

#define BAR_COLOR 0xffffff
#define BAR_FONT "JetBrains Mono Nerd Font:size11:style=bold"
#define BAR_FONT_SIZE 30
#define BAR_FONT_COLOR "#f8f8f8"
#define BAR_BORDER_WIDTH 1

#define BAR_TRUE_CENTER true // wont account for left/rigth segmenents and will put it(segment) exactly in themiddle

#define BAR_MODULE_PADDING 20 // padding from the sides of the bar
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
    .format = "VOL: %d%% >",
    .position = SEGMENT_RIGHT,
    .enabled = true
  },
  {
    .name = "brightness",
    .command = "brightnessctl get | awk -v max=$(brightnessctl max) '{print int($1*100/max)}'",
    .format = "BRT: %d%% >",
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
    .enabled = true
  },
  {
    .name = "desktop",
    .format = "%s",
    .position = SEGMENT_LEFT,
    .enabled = true
  }
};
