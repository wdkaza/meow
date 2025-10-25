#pragma once
#include <X11/X.h> // ignore
#include <stdint.h> // ignore
#include <X11/keysym.h> // ignore
#include <stdbool.h>  // ignore
#include <X11/Xlib.h> // ignore

// change the values below if needed
#define VOLUME_UP_CMD "pactl set-sink-volume @DEFAULT_SINK@ +5%"
#define VOLUME_DOWN_CMD "pactl set-sink-volume @DEFAULT_SINK@ -5%"
#define VOLUME_MUTE_CMD "pactl set-sink-mute @DEFAULT_SINK@ toggle"
#define BRIGHTNESS_UP_CMD "brightnessctl set +5%"
#define BRIGHTNESS_DOWN_CMD "brightnessctl set 5%-"
#define BRIGHTNESS_MIN_CMD "brightnessctl set 1%"
// keys names
typedef enum {
  KeyVoidSymbol = XK_VoidSymbol,
  KeyBackSpace = XK_BackSpace,
  KeyTab = XK_Tab,
  KeyLinefeed = XK_Linefeed,
  KeyClear = XK_Clear,
  KeyReturn = XK_Return,
  KeyPause = XK_Pause,
  KeyScroll_Lock = XK_Scroll_Lock,
  KeySys_Req = XK_Sys_Req,
  KeyEscape = XK_Escape,
  KeyDelete = XK_Delete,
  KeyHome = XK_Home,
  KeyLeft = XK_Left,
  KeyUp = XK_Up,
  KeyRight = XK_Right,
  KeyDown = XK_Down,
  KeyPage_Up = XK_Page_Up,
  KeyPage_Down = XK_Page_Down,
  KeyEnd = XK_End,
  KeyBegin = XK_Begin,
  KeySpace = XK_space,
  KeyExclam = XK_exclam,
  KeyQuotedbl = XK_quotedbl,
  KeyNumberSign = XK_numbersign,
  KeyDollar = XK_dollar,
  KeyPercent = XK_percent,
  KeyAmpersand = XK_ampersand,
  KeyApostrophe = XK_apostrophe,
  KeyParenleft = XK_parenleft,
  KeyParenright = XK_parenright,
  KeyAsterisk = XK_asterisk,
  KeyPlus = XK_plus,
  KeyComma = XK_comma,
  KeyMinus = XK_minus,
  KeyPeriod = XK_period,
  KeySlash = XK_slash,
  Key0 = XK_0,
  Key1 = XK_1,
  Key2 = XK_2,
  Key3 = XK_3,
  Key4 = XK_4,
  Key5 = XK_5,
  Key6 = XK_6,
  Key7 = XK_7,
  Key8 = XK_8,
  Key9 = XK_9,
  KeyColon = XK_colon,
  KeySemicolon = XK_semicolon,
  KeyLess = XK_less,
  KeyEqual = XK_equal,
  KeyGreater = XK_greater,
  KeyQuestion = XK_question,
  KeyAt = XK_at,
  KeyBracketLeft = XK_bracketleft,
  KeyBackslash = XK_backslash,
  KeyBracketRight = XK_bracketright,
  KeyAsciiCircum = XK_asciicircum,
  KeyUnderscore = XK_underscore,
  KeyGrave = XK_grave,
  KeyA = XK_a,
  KeyB = XK_b,
  KeyC = XK_c,
  KeyD = XK_d,
  KeyE = XK_e,
  KeyF = XK_f,
  KeyG = XK_g,
  KeyH = XK_h,
  KeyI = XK_i,
  KeyJ = XK_j,
  KeyK = XK_k,
  KeyL = XK_l,
  KeyM = XK_m,
  KeyN = XK_n,
  KeyO = XK_o,
  KeyP = XK_p,
  KeyQ = XK_q,
  KeyR = XK_r,
  KeyS = XK_s,
  KeyT = XK_t,
  KeyU = XK_u,
  KeyV = XK_v,
  KeyW = XK_w,
  KeyX = XK_x,
  KeyY = XK_y,
  KeyZ = XK_z,
  KeyBraceLeft = XK_braceleft,
  KeyBar = XK_bar,
  KeyBraceRight = XK_braceright,
  KeyAsciiTilde = XK_asciitilde,
  KeyNum_Lock = XK_Num_Lock,
  KeyKP_0 = XK_KP_0,
  KeyKP_1 = XK_KP_1,
  KeyKP_2 = XK_KP_2,
  KeyKP_3 = XK_KP_3,
  KeyKP_4 = XK_KP_4,
  KeyKP_5 = XK_KP_5,
  KeyKP_6 = XK_KP_6,
  KeyKP_7 = XK_KP_7,
  KeyKP_8 = XK_KP_8,
  KeyKP_9 = XK_KP_9,
  KeyKP_Decimal = XK_KP_Decimal,
  KeyKP_Divide = XK_KP_Divide,
  KeyKP_Multiply = XK_KP_Multiply,
  KeyKP_Subtract = XK_KP_Subtract,
  KeyKP_Add = XK_KP_Add,
  KeyKP_Enter = XK_KP_Enter,
  KeyKP_Equal = XK_KP_Equal,
  KeyF1 = XK_F1,
  KeyF2 = XK_F2,
  KeyF3 = XK_F3,
  KeyF4 = XK_F4,
  KeyF5 = XK_F5,
  KeyF6 = XK_F6,
  KeyF7 = XK_F7,
  KeyF8 = XK_F8,
  KeyF9 = XK_F9,
  KeyF10 = XK_F10,
  KeyF11 = XK_F11,
  KeyF12 = XK_F12,
  KeyF13 = XK_F13,
  KeyF14 = XK_F14,
  KeyF15 = XK_F15,
  KeyF16 = XK_F16,
  KeyF17 = XK_F17,
  KeyF18 = XK_F18,
  KeyF19 = XK_F19,
  KeyF20 = XK_F20,
  KeyF21 = XK_F21,
  KeyF22 = XK_F22,
  KeyF23 = XK_F23,
  KeyF24 = XK_F24,
  KeyF25 = XK_F25,
  KeyF26 = XK_F26,
  KeyF27 = XK_F27,
  KeyF28 = XK_F28,
  KeyF29 = XK_F29,
  KeyF30 = XK_F30,
  KeyF31 = XK_F31,
  KeyF32 = XK_F32,
  KeyF33 = XK_F33,
  KeyF34 = XK_F34,
  KeyF35 = XK_F35,
  KeyShift_L = XK_Shift_L,
  KeyShift_R = XK_Shift_R,
  KeyControl_L = XK_Control_L,
  KeyControl_R = XK_Control_R,
  KeyCaps_Lock = XK_Caps_Lock,
  KeyShift_Lock = XK_Shift_Lock,
  KeyMeta_L = XK_Meta_L,
  KeyMeta_R = XK_Meta_R,
  KeyAlt_L = XK_Alt_L,
  KeyAlt_R = XK_Alt_R,
  KeySuper_L = XK_Super_L,
  KeySuper_R = XK_Super_R,
  KeyHyper_L = XK_Hyper_L,
  KeyHyper_R = XK_Hyper_R,
} keycode_t;

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
void setWindowLayoutHorizontal(Arg *arg);
void moveSlavesStackForward(Arg *arg);
void moveSlavesStackBack(Arg *arg);
void moveWindowUp(Arg *arg);
void moveWindowDown(Arg *arg);
void swapSlaveWithMaster(Arg *arg);
void fullscreen(Arg *arg);
void disableBar(Arg *arg);
void enableBar(Arg *arg);
void cycleWindows(Arg *arg);
void cycleWindowsBackwards(Arg *arg);
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


