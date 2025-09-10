#pragma once
#include "structs.h"

#define START_WINDOW_GAP 20
#define DESKTOP_COUNT 10
#define BORDER_WIDTH 2
#define BORDER_FOCUSED_WIDTH 2 // currently a bit broken visually(layout will look ugly)
#define BORDER_COLOR 0x03819B
#define BORDER_FOCUSED_COLOR 0xA54242

#define BG_COLOR 0x000000

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
  //{MOD, XK_0,      switchDesktop,           {.i = 0}},

  {MOD|ShiftMask, XK_1, transferWindowToDesktop, {.i = 1}},
  {MOD|ShiftMask, XK_2, transferWindowToDesktop, {.i = 2}},
  {MOD|ShiftMask, XK_3, transferWindowToDesktop, {.i = 3}},
  {MOD|ShiftMask, XK_4, transferWindowToDesktop, {.i = 4}},
  {MOD|ShiftMask, XK_5, transferWindowToDesktop, {.i = 5}},
  {MOD|ShiftMask, XK_6, transferWindowToDesktop, {.i = 6}},
  {MOD|ShiftMask, XK_7, transferWindowToDesktop, {.i = 7}},
  {MOD|ShiftMask, XK_8, transferWindowToDesktop, {.i = 8}},
  {MOD|ShiftMask, XK_9, transferWindowToDesktop, {.i = 9}},
  //{MOD|ShiftMask, XK_0, transferWindowToDesktop, {.i = 0}}
};


// you probably shouldnt touch these, those are to avoid magic numbers

#define REFRESH_TIME 1
