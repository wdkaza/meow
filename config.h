#pragma once
#include <X11/X.h>
#include <limits.h>
#include <stdbool.h> // xD ignore this way of doing things 
#include "structs.h"

#define START_WINDOW_GAP 20
#define DESKTOP_COUNT 10
#define BORDER_WIDTH 2
#define BORDER_FOCUSED_WIDTH 2 // currently a bit broken visually(will look ugly)
#define BORDER_COLOR 0x1e1e1e
#define BORDER_FOCUSED_COLOR 0xADD8E6
#define XRESOURCES_AUTO_RELOAD true
#define CLAMP_FLOATING_WINDOWS true // wont let floating windows go outside of screen
#define AUTOMATICLY_PUT_FLOATING_WINDOWS_INTO_LAYOUT true // (WIP) bad name btw, will change later xD

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
// spawn                  (value required), (pretty much executes commands so can be used for them too like adjusting volume)
// kill
// exitWM
// addToLayout
// moveWindowUp           (inLayout)
// moveWindowDown         (inLayout)
// swapSlaveWithMaster
// increaseVolume
// decreaseVolume
// muteVolume
// increaseBrightness     (you can use "spawn" with your custom command if this one wont work, also do the same for others if they do not work for you)
// decreaseBrigthness     
// minBrigthness (1%)
// increaseGapSize        (value required) value = step size (default 5)
// decreaseGapSize        (value required) value = step size (default 5)
// increaseMasterGapSize  (value required) value = step size (default 40)
// decreaseMasterGapSize  (value required) value = step size (default 40)
// increaseSlaveHeight    (value required) value = step size
// decreaseSlaveHeight    (value required) value = step size
// moveSlaveStackForward
// moveSlaveStackBack
// resetMasterGapSize     (sets the gap to 0, master window takes half ot the screen)
// cycleWindows
// fullscreen             
// setWindowLayoutTiled
//
// switchDesktop          (value required) value = desktop number
// transferWindowToDesktop(value required) value = desktop number

// [MOD-KEY, KEY, ACTION, VALUE(if none set to 0)]
// [MOD-KEY|ShiftMask,   ...]   for mod+shift keybinds
// [MOD-KEY|ControlMask, ...]   for mod+control keybinds

// IMPORTANT !!!
// you can find the keys naming in structs.h file
// IMPORTANT !!!

struct KeyEvent keys[] = {
  //
  // {MOD, XK_F3,  spawn,                   {.v = (const char *[]){"pactl", "set-sink-volume", "@DEFAULT_SINK", "+5%", NULL}}}
  // split args by coma and double quote them "arg"
  // ^^^ example of custom command ^^^
  // keeping it simple by using increaseVolume/decreaseVolume/muteVolume, etc instead of "spawn"
  //
  {MOD, KeyReturn, spawn,                   {.v = terminal}},
  {MOD, KeySlash,  spawn,                   {.v = launcher}},
  {MOD, KeyP,      spawn,                   {.v = screenshot}},
  {MOD, KeyQ,      kill,                    {0}},
  {MOD, KeySpace,  addWindowToLayout,       {0}},
  {MOD, KeyUp,     moveWindowUp,            {0}},
  {MOD, KeyDown,   moveWindowDown,          {0}},
  {MOD, KeyE,      swapSlaveWithMaster,     {0}},
  {MOD, KeyF3,     increaseVolume,          {0}},
  {MOD, KeyF2,     decreaseVolume,          {0}},
  {MOD, KeyF1,     muteVolume,              {0}},
  {MOD, KeyF6,     increaseBrightness,      {0}},
  {MOD, KeyF5,     decreaseBrightness,      {0}},
  {MOD, KeyF7,     minBrightness,           {0}},
  {MOD, KeyTab,    cycleWindows,            {0}},
  {MOD, KeyF,      fullscreen,              {0}},
  {MOD, KeyR,      setWindowLayoutTiled,    {0}},
  {MOD, KeyT,      setWindowLayoutFloating, {0}},
  {MOD, KeyO,      setWindowLayoutCascase,  {0}},

  {MOD|ShiftMask,KeyQ, exitWM,              {0}},



  // Layout related keybindings
  {MOD, KeyY,           increaseGapSize,       {.i = 5}},
  {MOD, KeyU,           decreaseGapSize,       {.i = 5}},
  {MOD|ShiftMask, KeyY, increaseMasterGapSize, {.i = 40}},
  {MOD|ShiftMask, KeyU, decreaseMasterGapSize, {.i = 40}},
  {MOD|ShiftMask, KeyS, increaseSlaveHeight,   {.i = 20}},
  {MOD|ShiftMask, KeyD, decreaseSlaveHeight,   {.i = 20}},
  {MOD|ShiftMask, KeyO, resetMasterGapSize,    {0}},
  {MOD|ShiftMask, KeyW, moveSlavesStackForward,{0}},
  {MOD|ShiftMask, KeyE, moveSlavesStackBack,   {0}},


  // desktop related keybindings
  {MOD, Key1,      switchDesktop,           {.i = 1}},
  {MOD, Key2,      switchDesktop,           {.i = 2}},
  {MOD, Key3,      switchDesktop,           {.i = 3}},
  {MOD, Key4,      switchDesktop,           {.i = 4}},
  {MOD, Key5,      switchDesktop,           {.i = 5}},
  {MOD, Key6,      switchDesktop,           {.i = 6}},
  {MOD, Key7,      switchDesktop,           {.i = 7}},
  {MOD, Key8,      switchDesktop,           {.i = 8}},
  {MOD, Key9,      switchDesktop,           {.i = 9}},

  {MOD|ShiftMask, Key1, transferWindowToDesktop, {.i = 1}},
  {MOD|ShiftMask, Key2, transferWindowToDesktop, {.i = 2}},
  {MOD|ShiftMask, Key3, transferWindowToDesktop, {.i = 3}},
  {MOD|ShiftMask, Key4, transferWindowToDesktop, {.i = 4}},
  {MOD|ShiftMask, Key5, transferWindowToDesktop, {.i = 5}},
  {MOD|ShiftMask, Key6, transferWindowToDesktop, {.i = 6}},
  {MOD|ShiftMask, Key7, transferWindowToDesktop, {.i = 7}},
  {MOD|ShiftMask, Key8, transferWindowToDesktop, {.i = 8}},
  {MOD|ShiftMask, Key9, transferWindowToDesktop, {.i = 9}},
};


// you probably shouldnt touch these, those are to avoid magic numbers

#define REFRESH_TIME 1
