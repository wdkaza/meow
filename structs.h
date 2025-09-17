#pragma once
#include <X11/X.h>
#include <stdint.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <X11/Xlib.h>

#define VOLUME_UP_CMD "pactl set-sink-volume @DEFAULT_SINK@ +5%"
#define VOLUME_DOWN_CMD "pactl set-sink-volume @DEFAULT_SINK@ -5%"
#define VOLUME_MUTE_CMD "pactl set-sink-mute @DEFAULT_SINK@ toggle"
#define BRIGHTNESS_UP_CMD "brightnessctl set +5%"
#define BRIGHTNESS_DOWN_CMD "brightnessctl set 5%-"
#define BRIGHTNESS_MIN_CMD "brightnessctl set 1%"

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
void exitWM(Arg *arg);
void decreaseGapSize(Arg *arg);
void increaseGapSize(Arg *arg);
void increaseMasterGapSize(Arg *arg);
void decreaseMasterGapSize(Arg *arg);
void increaseSlaveHeight(Arg *arg);
void decreaseSlaveHeight(Arg *arg);
void resetMasterGapSize(Arg *arg);
void setWindowLayoutTiled(Arg *arg);
void setWindowLayoutFloating(Arg *arg);
void setWindowLayoutCascase(Arg *arg);
void moveSlavesStackForward(Arg *arg);
void moveSlavesStackBack(Arg *arg);
void moveWindowUp(Arg *arg);
void moveWindowDown(Arg *arg);
void swapSlaveWithMaster(Arg *arg);
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
