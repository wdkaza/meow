#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>
#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

// thanks @cococry for some of the code
// @cococry also insipered me to try to code this 
// like actually massive props to cococry's old code and dwm's code 
// it helpmed me so much to understand how to even start making a wm

#define CLIENT_WINDOW_CAP 256

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) ((a) > (b) ? (a) : b)

typedef enum{
  WINDOW_LAYOUT_TILED_MASTER = 0,
  WINDOW_LAYOUT_FLOATING
} WindowLayout;

typedef struct {
  float x, y;
} Vec2;

typedef struct {
  Window win;
  Window frame;
  bool fullscreen;
  Vec2 fullscreenRevertSize;
  bool inLayout;
  int32_t desktopIndex;
} Client;

typedef struct {
  Window win;
  bool hidden;
  char barText[256];
  XftFont *font;
  XftColor color;
  XftDraw* draw;
} Bar;

typedef struct {
  Display* display;
  Window root;

  bool running;

  Client client_windows[CLIENT_WINDOW_CAP];
  uint32_t clients_count;
  int32_t currentDesktop;

  Vec2 cursor_start_pos;
  Vec2 cursor_start_frame_pos;
  Vec2 cursor_start_frame_size;

  WindowLayout currentLayout;
  uint32_t windowGap;
  Window focused_Window;

  int screenHeight;
  int screenWidth;

  Bar bar;

  Atom ATOM_WM_DELETE_WINDOW; // remove later(useless mostly)
  Atom ATOM_WM_PROTOCOLS; // remove later(useless mostly)
} XWM;

static void handleConfigureRequst(XEvent *ev);
static void handleUnmapNotify(XEvent *ev);
static void handleDestroyNotify(XEvent *ev);
static void handleConfigureNotify(XEvent *ev);
static void handleMapRequest(XEvent *ev);
static void xwmWindowFrame(Window win, bool createdBeforeWindowManager);
static void xwmWindowUnframe(Window w);
static void handleButtonPress(XEvent *ev);
static void handleKeyRelease(XEvent *ev);
static void handleMotionNotify(XEvent *ev);
static void handleKeyPress(XEvent *ev);
static void retileLayout();
static void sendClientMessage(Window w, Atom a);
static void moveClientUpLayout(Client *client);
static void moveClientDownLayout(Client *client);
static void moveWindowToDesktop(Window w, int32_t desktopIndex);
static void changeDesktop(int32_t desktopIndex);
static Window getFrameWindow(Window w);
static int32_t getClientIndex(Window w);
static void setFocusToWindow(Window w);
static bool clientFrameExists(Window w);
static bool clientWindowExists(Window w);
static void unsetFullscreen(Window w);
static void initDesktops();
static void setFullscreen(Window w);
static Atom* findAtomPtrRange(Atom* ptr1, Atom* ptr2, Atom val);
static void initBar();
static void updateBar();
static void hideBar();
static void unhideBar();

static XWM wm;

int xwm_error_handler(Display *dpy, XErrorEvent *e){
  if(e->error_code == BadWindow || e->error_code == BadDrawable || e->error_code == BadMatch){
    return 0;
  }

  char buf[1024];
  XGetErrorText(dpy, e->error_code, buf, sizeof(buf));
  fprintf(stderr, "x error: %s (request %d, resource: %lu)\n", buf, e->error_code, e->resourceid);
  return 0;
}

void initDesktops(){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex < 0 || wm.client_windows[i].desktopIndex >= DESKTOP_COUNT){
      wm.client_windows[i].desktopIndex = 0;
    }
  }
}

void initBar(){
  wm.bar.hidden = false;
  wm.bar.win = XCreateSimpleWindow(wm.display,
                                   wm.root,
                                   0, 0, 
                                   wm.screenWidth - (2 * BORDER_WIDTH), BAR_HEIGHT, 
                                   BORDER_WIDTH, BORDER_COLOR,
                                   BAR_COLOR); // TODO : add padding customization later


  wm.bar.draw = XftDrawCreate(wm.display,
                              wm.bar.win,
                              DefaultVisual(wm.display, DefaultScreen(wm.display)),
                              DefaultColormap(wm.display, DefaultScreen(wm.display)));

  wm.bar.font = XftFontOpenName(wm.display, DefaultScreen(wm.display), BAR_FONT);

  XftColorAllocName(
    wm.display,
    DefaultVisual(wm.display, DefaultScreen(wm.display)),
    DefaultColormap(wm.display, DefaultScreen(wm.display)),
    BAR_FONT_COLOR,
    &wm.bar.color);
  XSelectInput(wm.display, wm.bar.win, SubstructureRedirectMask | SubstructureNotifyMask);
  XMapWindow(wm.display, wm.bar.win);
  XRaiseWindow(wm.display, wm.bar.win);
}

XWM xwm_init(){
  XWM wm = {0};
  XSetErrorHandler(xwm_error_handler);

  wm.display = XOpenDisplay(NULL);
  if(!wm.display){
    printf("failed to open x display\n");
    exit(1);
  }


  wm.root = DefaultRootWindow(wm.display);
  int screen = DefaultScreen(wm.display);
  wm.screenWidth = DisplayWidth(wm.display, screen);
  wm.screenHeight = DisplayHeight(wm.display, screen);

  wm.focused_Window = None;

  XSetWindowAttributes attrs;
  attrs.backing_store = Always;
  XChangeWindowAttributes(wm.display, wm.root, CWBackingStore, &attrs);
  XSetCloseDownMode(wm.display, RetainPermanent);
  XSync(wm.display, false);

  return wm;
}

void xwm_run(){
  wm.running = true;

  Cursor cursor = XCreateFontCursor(wm.display, XC_left_ptr);
  XDefineCursor(wm.display, wm.root, cursor);
  XSync(wm.display, false);

  XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(wm.display, false);

  XGrabKey(wm.display, XKeysymToKeycode(wm.display, KILL_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, ROFI_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, TERMINAL_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, FULLSCREEN_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync); 
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, CYCLE_WINDOWS_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, GAP_INCREASE_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, GAP_DECREAS_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_TILED_MASTER), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_FLOATING), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, WINDOW_ADD_TO_LAYOUT), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, WINDOW_LAYOUT_MOVE_UP), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, WINDOW_LAYOUT_MOVE_DOWN), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, BAR_HIDE_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, BAR_SHOW_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  for (int i = 0; i < 10; i++){
    KeyCode keycode = XKeysymToKeycode(wm.display, XK_0 + i);
    XGrabKey(wm.display, keycode, MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm.display, keycode, MASTER_KEY | ShiftMask, wm.root, false, GrabModeAsync, GrabModeAsync);
  }
  
  
  wm.clients_count = 0;
  wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.windowGap = START_WINDOW_GAP;
  wm.currentLayout = WINDOW_LAYOUT_TILED_MASTER;
  wm.ATOM_WM_PROTOCOLS = XInternAtom(wm.display, "WM_PROTOCOLS", false);
  wm.ATOM_WM_DELETE_WINDOW = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);

  initBar();
  initDesktops();

  while(wm.running){
    if(XPending(wm.display)){
      XEvent ev;
      XNextEvent(wm.display, &ev);

      switch(ev.type){
        default:
          break;
        case MapRequest:
          handleMapRequest(&ev);
          break;
        case UnmapNotify:
          handleUnmapNotify(&ev);
          break;
        case DestroyNotify:
          handleDestroyNotify(&ev);
          break;
        case ConfigureNotify:
          handleConfigureNotify(&ev);
          break;
        case ConfigureRequest:
          handleConfigureRequst(&ev);
          break;
       case ButtonPress:
          handleButtonPress(&ev);
          break;
        case MotionNotify:
          handleMotionNotify(&ev);
          break;
        case KeyPress:
          handleKeyPress(&ev);
          break;
        case KeyRelease:
          handleKeyRelease(&ev);
          break;
      }
      updateBar(); // temporary because it probably eats alot of recourses
    }
  }
}

void handleButtonPress(XEvent *ev){
  XButtonEvent *e = &ev->xbutton;

  if(!clientWindowExists(e->window)) return;

  Window targetWindow = e->window;

  if(clientWindowExists(targetWindow) || clientFrameExists(e->window)){
    setFocusToWindow(targetWindow);
  }

  if((e->state & MASTER_KEY) && e->button == Button1){
    wm.cursor_start_pos = (Vec2){
      .x = (float)e->x_root,
      .y = (float)e->y_root
    };

    XWindowAttributes attr;
    Window frame = getFrameWindow(e->window);
    XGetWindowAttributes(wm.display, frame, &attr);

    wm.cursor_start_frame_pos = (Vec2){
      .x = (float)attr.x,
      .y = (float)attr.y
    };
    wm.cursor_start_frame_size = (Vec2){
      .x = (float)attr.width,
      .y = (float)attr.height
    };
  }
}

void handleMotionNotify(XEvent *ev){
  XMotionEvent *e = &ev->xmotion;

  if(!clientWindowExists(e->window)) return;

  int dx = e->x_root - (int)wm.cursor_start_pos.x;
  int dy = e->y_root - (int)wm.cursor_start_pos.y;

  Window frame = getFrameWindow(e->window);

  Client *client = &wm.client_windows[getClientIndex(e->window)];
  if((e->state & MASTER_KEY) && (e->state & Button1Mask)){
    XMoveWindow(wm.display, frame, 
                (int)(wm.cursor_start_frame_pos.x) + dx, 
                (int)(wm.cursor_start_frame_pos.y) + dy
    );
    if(client->fullscreen){
      client->fullscreen = false;
      XSetWindowBorderWidth(wm.display, client->frame, BORDER_WIDTH);
    }
    if(client->inLayout){
      client->inLayout = false;
      retileLayout();
    }
  }
  else if((e->state & MASTER_KEY) && (e->state & Button3Mask)){
    /*
    Vec2 rdy = (Vec2){.x = wm.cursor_start_frame_size.x + MAX(dx, -wm.cursor_start_frame_size.x),
                      .y = wm.cursor_start_frame_size.y + MAX(dy, -wm.cursor_start_frame_size.y)};
    XResizeWindow(wm.display, frame, (int)rdy.x, (int)rdy.y);
    XResizeWindow(wm.display, e->window, (int)rdy.x, (int)rdy.y);
    */
    int new_width = MAX(1, (int)wm.cursor_start_frame_size.x + dx);
    int new_height = MAX(1, (int)wm.cursor_start_frame_size.y + dy);
    
    XResizeWindow(wm.display, frame, new_width, new_height);
    XResizeWindow(wm.display, e->window, new_width, new_height);

    if(client->inLayout){
      client->inLayout = false;
      retileLayout();
    }
  }

}

void updateWindowBorders(Window w){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop){
      XSetWindowBorder(wm.display, wm.client_windows[i].frame, BORDER_COLOR);
      XSetWindowBorderWidth(wm.display, wm.client_windows[i].frame, BORDER_WIDTH);
    }
  }

  if(w != None && clientWindowExists(w)){
    Window frame = getFrameWindow(w);
    if(frame != None){
      XSetWindowBorder(wm.display, frame, BORDER_FOCUSED_COLOR);
      XSetWindowBorderWidth(wm.display, frame, BORDER_FOCUSED_WIDTH);
    }
  }

  wm.focused_Window = w;
  XSync(wm.display, false);

}


void setFocusToWindow(Window w){
  if(w != None && clientWindowExists(w)){
    XRaiseWindow(wm.display, getFrameWindow(w));
    XSetInputFocus(wm.display, w, RevertToParent, CurrentTime);
    updateWindowBorders(w);
  }
  else{
    XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
    updateWindowBorders(None);
  }
}

void focusNextWindow(){
  if(wm.clients_count == 0){
    XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
    return;
  }

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop){
      setFocusToWindow(wm.client_windows[i].win);
      return;
    }
  }
  XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
  updateWindowBorders(None);
}

void handleConfigureRequst(XEvent *ev){
  XConfigureRequestEvent *e = &ev->xconfigurerequest;
  XWindowChanges changes;

  changes.x = e->x;
  changes.y = e->y;
  changes.width = e->width;
  changes.height = e->height;
  changes.sibling = e->above;
  changes.stack_mode = e->detail;
  if(clientWindowExists(e->window)){
    Window frame_win = getFrameWindow(e->window);
    if(frame_win != 0){
    XConfigureWindow(wm.display, frame_win, e->value_mask, &changes);
    XConfigureWindow(wm.display, e->window, e->value_mask, &changes);
    }
  }
  XConfigureWindow(wm.display, e->window, e->value_mask, &changes);
}

void handleConfigureNotify(XEvent *ev){
  XConfigureEvent *e = &ev->xconfigure;
  if(!clientFrameExists(e->window)) return;
  Window frame = getFrameWindow(e->window);
  XMoveResizeWindow(wm.display, frame, e->x, e->y, e->width, e->height);
}

void handleKeyPress(XEvent *ev){
  XKeyEvent *e = &ev->xkey;

  Window focusedWindow;
  int revert_to;
  XGetInputFocus(wm.display, &focusedWindow, &revert_to);

  if(focusedWindow == wm.root && focusedWindow == None){
    focusedWindow = wm.focused_Window;
  }

  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, KILL_KEY)){
    if(clientWindowExists(focusedWindow) && focusedWindow != wm.root){
      int32_t closingIndex = getClientIndex(focusedWindow);
    
      Atom* protocols;
      int32_t num_protocols;
      if(XGetWMProtocols(wm.display, focusedWindow, &protocols, &num_protocols) 
      && findAtomPtrRange(protocols, protocols + num_protocols, wm.ATOM_WM_DELETE_WINDOW) != protocols + num_protocols){
        sendClientMessage(focusedWindow, wm.ATOM_WM_DELETE_WINDOW);
        XFree(protocols);
      }
      else{
        XKillClient(wm.display, focusedWindow);
      }
      if(wm.clients_count > 1){
        int32_t nextIndex = (closingIndex + 1) % wm.clients_count;
        if(nextIndex == closingIndex){
          nextIndex = (closingIndex - 1 + wm.clients_count) % wm.clients_count;
        }
        int startIndex = nextIndex;
        while(wm.client_windows[nextIndex].desktopIndex != wm.currentDesktop){
          nextIndex = (nextIndex + 1) % wm.clients_count;
          if(nextIndex == startIndex) break;
        }
        if(wm.client_windows[nextIndex].desktopIndex == wm.currentDesktop){
          setFocusToWindow(wm.client_windows[nextIndex].win);
        }
        else{
          focusNextWindow();
        }
      }
      else{
        XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
      }
    }
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, TERMINAL_KEY)){
    system(TERMINAL_CMD);
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, ROFI_KEY)){
    system(ROFI_CMD);
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, GAP_INCREASE_KEY)){
    wm.windowGap = MIN(wm.windowGap + 2, 100);
    retileLayout();
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, GAP_DECREAS_KEY)){
    wm.windowGap = MAX((int)wm.windowGap - 2, 15);
    retileLayout();
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_TILED_MASTER)){
    wm.currentLayout = WINDOW_LAYOUT_TILED_MASTER;
    for(uint32_t i = 0; i < wm.clients_count; i++){
      wm.client_windows[i].inLayout = true;
      if(wm.client_windows[i].fullscreen){
        unsetFullscreen(wm.client_windows[i].frame);
      }
    }
    retileLayout();
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_FLOATING)){
    wm.currentLayout = WINDOW_LAYOUT_FLOATING;
  }
  else if((e->state & MASTER_KEY) && (e->state & ShiftMask) && XK_0 <= XkbKeycodeToKeysym(wm.display, e->keycode, 0, 0) && XkbKeycodeToKeysym(wm.display, e->keycode, 0, 0) <= XK_9){
    int newDesktop = XkbKeycodeToKeysym(wm.display, e->keycode, 0, 0) - XK_0;
    if(clientWindowExists(focusedWindow) && newDesktop < DESKTOP_COUNT){
      moveWindowToDesktop(focusedWindow, newDesktop);
    }
  }
  else if((e->state & MASTER_KEY) && XK_0 <= XkbKeycodeToKeysym(wm.display, e->keycode, 0, 0) && XkbKeycodeToKeysym(wm.display,e->keycode, 0, 0) <= XK_9){
    int newDesktop = XkbKeycodeToKeysym(wm.display, e->keycode, 0, 0) - XK_0;
    if(newDesktop != wm.currentDesktop && newDesktop < DESKTOP_COUNT){
      changeDesktop(newDesktop);
    }
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, WINDOW_ADD_TO_LAYOUT)){
    if(clientWindowExists(focusedWindow)){
      wm.client_windows[getClientIndex(focusedWindow)].inLayout = true;
      retileLayout();
    }
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_MOVE_UP)){
    moveClientUpLayout(&wm.client_windows[getClientIndex(focusedWindow)]);
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_MOVE_DOWN)){
    moveClientDownLayout(&wm.client_windows[getClientIndex(focusedWindow)]);
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, FULLSCREEN_KEY)){
    uint32_t clientIndex = getClientIndex(focusedWindow);
    if(!wm.client_windows[clientIndex].fullscreen){
      setFullscreen(focusedWindow);
    }
    else{
      unsetFullscreen(focusedWindow);
    }
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, BAR_HIDE_KEY)){
    hideBar();
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, BAR_SHOW_KEY)){
    unhideBar();
  }
  else if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, CYCLE_WINDOWS_KEY)){
    int32_t currentIndex = -1;
    for(uint32_t i = 0; i < wm.clients_count; i++){
      if((wm.client_windows[i].win == focusedWindow || wm.client_windows[i].frame == focusedWindow) && wm.client_windows[i].desktopIndex == wm.currentDesktop){
        currentIndex = i;
        break;
      }
    }
    if(currentIndex != -1){
      for(uint32_t i = 1; i < wm.clients_count; i++){
        uint32_t nextIndex = (currentIndex + i) % wm.clients_count;
        if(wm.client_windows[nextIndex].desktopIndex == wm.currentDesktop){
          setFocusToWindow(wm.client_windows[nextIndex].win);
          break;
        }
      }
    }
  }
}

void handleKeyRelease(XEvent *ev){
  (void)ev;
}

void sendClientMessage(Window w, Atom a){
  XEvent msg = {0};
  memset(&msg, 0, sizeof(msg));
  msg.xclient.type = ClientMessage;
  msg.xclient.window = w;
  msg.xclient.message_type = wm.ATOM_WM_PROTOCOLS;
  msg.xclient.format = 32;
  msg.xclient.data.l[0] = a;
  msg.xclient.data.l[1] = CurrentTime;
  XSendEvent(wm.display, w, false, NoEventMask, &msg);
}

void handleMapRequest(XEvent *ev){
  XMapRequestEvent *e = &ev->xmaprequest;
  xwmWindowFrame(e->window, false);
  XMapWindow(wm.display, e->window);

  Window frame = getFrameWindow(e->window);
  XRaiseWindow(wm.display, frame);
  setFocusToWindow(e->window);
  updateBar();
}

void handleUnmapNotify(XEvent *ev){
  XUnmapEvent *e = &ev->xunmap;
  if(!clientWindowExists(e->window) || e->event == wm.root){
    return;
  }

  if(e->window == wm.bar.win){
    return;
  }

  uint32_t closingIndex = getClientIndex(e->window);
  if(closingIndex == (uint32_t)-1) return;


  Window focusedWindow;
  int revert_to;
  XGetInputFocus(wm.display, &focusedWindow, &revert_to);
  bool was_focused = (focusedWindow == e->window);

  xwmWindowUnframe(e->window);

  if(was_focused && wm.clients_count > 0){
    focusNextWindow();
  }
  else if(wm.clients_count == 0){
    XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
    updateWindowBorders(None);
  }
  XSync(wm.display, false);
}

void handleDestroyNotify(XEvent *ev){
  (void)ev;
}

bool clientWindowExists(Window w){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].win == w){
      return true;
    }
  }
  return false;
}

bool clientFrameExists(Window w){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].frame == w){
      return true;
    }
  }
  return false;
}

Window getFrameWindow(Window w){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].win == w){
      return wm.client_windows[i].frame;
    }
  }
  return 0;
}


void xwmWindowUnframe(Window w){
  if(!clientWindowExists(w)) return;

  int32_t clientIndex = getClientIndex(w);
  if(clientIndex == -1) return;

  Window frameWin = wm.client_windows[clientIndex].frame;
  if(frameWin == 0) return;

  XUnmapWindow(wm.display, frameWin);
  XReparentWindow(wm.display, w, wm.root, 0, 0);
  XRemoveFromSaveSet(wm.display, w);
  XDestroyWindow(wm.display, frameWin);
  
  for(uint32_t j = clientIndex; j < wm.clients_count - 1; j++){
    wm.client_windows[j] = wm.client_windows[j+1];
  }
  
  //memset(&wm.client_windows[wm.clients_count -1], 0, sizeof(Client));
  wm.clients_count--;

  retileLayout();
  updateBar();
}


int32_t getClientIndex(Window w){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].win == w || wm.client_windows[i].frame == w){
      return i;
    }
  }
  return -1;
}

void setFullscreen(Window w){
  uint32_t clientIndex = getClientIndex(w);
  if(wm.client_windows[clientIndex].fullscreen) return;

  XWindowAttributes attr;
  XGetWindowAttributes(wm.display, w, &attr);
  wm.client_windows[clientIndex].fullscreen = true;
  wm.client_windows[clientIndex].fullscreenRevertSize = (Vec2){ .x = attr.width, .y = attr.height};

  Window frame = getFrameWindow(w);
  XSetWindowBorderWidth(wm.display, frame, BORDER_WIDTH);
  int screen = DefaultScreen(wm.display);
  int screenWidth = DisplayWidth(wm.display, screen);
  int screenHeight = DisplayHeight(wm.display, screen);
  // BUG HERE (full screen right now only works with gaps TODO: make actual fullscreen toggable) 
  XMoveResizeWindow(wm.display, frame, wm.windowGap - BORDER_WIDTH, wm.windowGap - BORDER_WIDTH, screenWidth - wm.windowGap * 2, screenHeight - wm.windowGap * 2);
  XResizeWindow(wm.display, w, screenWidth - wm.windowGap * 2, screenHeight - wm.windowGap * 2);
  hideBar();
}

void unsetFullscreen(Window w){
  uint32_t clientIndex = getClientIndex(w);
  if(!wm.client_windows[clientIndex].fullscreen) return;
  wm.client_windows[clientIndex].fullscreen = false;
  Window frame = getFrameWindow(w);

  int screen = DefaultScreen(wm.display);
  int screenWidth = DisplayWidth(wm.display, screen);
  int screenHeight = DisplayHeight(wm.display, screen);
  int windowWidth = wm.client_windows[clientIndex].fullscreenRevertSize.x;
  int windowHeight = wm.client_windows[clientIndex].fullscreenRevertSize.y;
  
  int centerX = ((screenWidth - windowWidth) / 2) - BORDER_WIDTH;
  int centerY = ((screenHeight - windowHeight) / 2) - BORDER_WIDTH;
  
  XMoveResizeWindow(wm.display, frame, centerX, centerY, windowWidth, windowHeight);
  XResizeWindow(wm.display, w, windowWidth, windowHeight);
  unhideBar();
}

void hideBar(){
  if(wm.bar.hidden) return;
  XUnmapWindow(wm.display, wm.bar.win);
  wm.bar.hidden = true;
  retileLayout();
}

void unhideBar(){
  if(!wm.bar.hidden) return;
  XMapWindow(wm.display, wm.bar.win);
  wm.bar.hidden = false;

  updateBar();
  retileLayout();
}

void updateBar(){
  if(wm.bar.hidden) return;
  XClearWindow(wm.display, wm.bar.win);

  snprintf(wm.bar.barText, sizeof(wm.bar.barText),
           "Desktop %d | Windows: %d | Layout: %s",
           wm.currentDesktop,
           wm.clients_count,
           wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER ? "Tiled" : "Floating");

  XftDrawStringUtf8(wm.bar.draw, &wm.bar.color, wm.bar.font, 10, wm.bar.font->ascent + 5, (XftChar8*)wm.bar.barText, strlen(wm.bar.barText));

  XSync(wm.display, False);
}

void changeDesktop(int32_t desktopIndex){
  if(desktopIndex < 0 || desktopIndex >= DESKTOP_COUNT) return;

  wm.currentDesktop = desktopIndex;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    Client *c = &wm.client_windows[i];
    if(c->desktopIndex == desktopIndex){
      XMapWindow(wm.display, c->frame);
    }
    else{
      XUnmapWindow(wm.display, c->frame);
    }
  }
  retileLayout();
  updateBar();
  focusNextWindow();
}

void moveWindowToDesktop(Window w, int32_t desktopIndex){
  if(desktopIndex < 0 || desktopIndex >= DESKTOP_COUNT) return;

  int32_t clientIndex = getClientIndex(w);
  if(clientIndex == -1) return;
  Client *client = &wm.client_windows[clientIndex];
  int32_t oldDesktop = client->desktopIndex;
  client->desktopIndex = desktopIndex;

  if(desktopIndex == wm.currentDesktop){
    XMapWindow(wm.display, client->frame);
  }
  else{
    XUnmapWindow(wm.display, client->frame);
  }

  retileLayout();

  if(oldDesktop == wm.currentDesktop && desktopIndex != wm.currentDesktop){
    focusNextWindow();
  }
}

Atom* findAtomPtrRange(Atom* ptr1, Atom* ptr2, Atom val){
  while(ptr1 < ptr2){
    if(*ptr1 == val){
      return ptr1;
    }
    ptr1++;
  }
  return ptr2;
}
// both functions below broken 
void moveClientUpLayout(Client *client){ // bug : ignores multiple desktops
  int32_t clientIndex = getClientIndex(client->win);
  if(clientIndex == -1 || wm.clients_count <= 1) return;

  uint32_t upperIndex = (clientIndex == (int32_t)wm.clients_count -1) ? 0 : clientIndex + 1;
  Client tmp = wm.client_windows[clientIndex];
  wm.client_windows[clientIndex] = wm.client_windows[upperIndex];
  wm.client_windows[upperIndex] = tmp;
  retileLayout();
}

void moveClientDownLayout(Client *client){ // bug : ignores multiple desktops
  int32_t clientIndex = getClientIndex(client->win);

  if(clientIndex == -1 || wm.clients_count <= 1) return;

  uint32_t lowerIndex = (clientIndex == 0) ? wm.clients_count - 1 : (uint32_t)(clientIndex - 1);
  Client tmp = wm.client_windows[clientIndex];
  wm.client_windows[clientIndex] = wm.client_windows[lowerIndex];
  wm.client_windows[lowerIndex] = tmp;
  retileLayout();
}

void retileLayout(){

  Client *clients[CLIENT_WINDOW_CAP];
  uint32_t client_count = 0;
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].inLayout && wm.client_windows[i].desktopIndex == wm.currentDesktop){
      clients[client_count++] = &wm.client_windows[i];
    }
  }
  if(client_count == 0) return;

  int availableHeight = wm.screenHeight - (wm.bar.hidden ? 0 : BAR_HEIGHT);
  int startY = wm.bar.hidden ? 0 : BAR_HEIGHT;

  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER){


    Client *master = clients[0];
    if(client_count == 1){
      XMoveWindow(wm.display, master->frame, wm.windowGap - BORDER_WIDTH, startY + wm.windowGap - BORDER_WIDTH);
      int frameWidth = wm.screenWidth - 2 * wm.windowGap;
      int frameHeight = availableHeight - 2 * wm.windowGap;
      XResizeWindow(wm.display, master->frame, frameWidth, frameHeight);
      XResizeWindow(wm.display, master->win, frameWidth, frameHeight);
      XSetWindowBorderWidth(wm.display, master->frame, BORDER_WIDTH);
      if(master->fullscreen){
        master->fullscreen = false;
      }
      return;
    } 
    XMoveWindow(wm.display, master->frame, wm.windowGap - BORDER_WIDTH, startY + wm.windowGap - BORDER_WIDTH);
    int masterFrameWidth = wm.screenWidth/2 - wm.windowGap - (wm.windowGap/2);
    int masterFrameHeight = availableHeight - 2 * wm.windowGap;
    XResizeWindow(wm.display, master->frame, masterFrameWidth, masterFrameHeight);
    XResizeWindow(wm.display, master->win, masterFrameWidth, masterFrameHeight);
    XSetWindowBorderWidth(wm.display, master->frame, BORDER_WIDTH);
    master->fullscreen = false;

    int32_t stackFrameWidth = wm.screenWidth/2 - wm.windowGap - (wm.windowGap/2);
    int32_t stackFrameHeight = (availableHeight - (client_count * wm.windowGap)) / (client_count - 1);
    int32_t stackX = wm.screenWidth / 2 + (wm.windowGap/2);

    for(uint32_t i = 1; i < client_count; i++){
      Client *client = clients[i]; 
      int32_t stackY = wm.windowGap + (i - 1) * (stackFrameHeight + wm.windowGap);
      int barOffset = wm.bar.hidden ? 0 : BAR_HEIGHT;
      XMoveWindow(wm.display, client->frame, stackX - BORDER_WIDTH, stackY + barOffset - BORDER_WIDTH);
      XResizeWindow(wm.display, client->frame, stackFrameWidth, stackFrameHeight);
      XResizeWindow(wm.display, client->win, stackFrameWidth, stackFrameHeight);
      XSetWindowBorderWidth(wm.display, client->frame, BORDER_WIDTH);
      if(client->fullscreen){
        client->fullscreen = false;
      }
    }

    if(wm.focused_Window != None && clientWindowExists(wm.focused_Window)){
      updateWindowBorders(wm.focused_Window);
    }
    updateBar();
    XSync(wm.display, false);
  } 
}

void xwmWindowFrame(Window win, bool createdBeforeWindowManager){
  XWindowAttributes attribs;

  assert(XGetWindowAttributes(wm.display, win, &attribs) && "failed to get window attributes");

  if(createdBeforeWindowManager){
    if(attribs.override_redirect || attribs.map_state != IsViewable){
      return; // ignore
    }
  }

  int window_x = attribs.x;
  int window_y = attribs.y;
  
  if(wm.currentLayout == WINDOW_LAYOUT_FLOATING){
    window_x = (wm.screenWidth - attribs.width) / 2;
    window_y = (wm.screenHeight - attribs.height) / 2;
  }


  Window winFrame = XCreateSimpleWindow(
    wm.display,
    wm.root,
    window_x,
    window_y,
    attribs.width,
    attribs.height,
    BORDER_WIDTH,
    BORDER_COLOR,
    BG_COLOR
  );

  XReparentWindow(wm.display, win, winFrame, 0, 0);

  XSelectInput(
    wm.display,
    winFrame,
    SubstructureRedirectMask |
    SubstructureNotifyMask
  );

  XAddToSaveSet(wm.display, win);
  XResizeWindow(wm.display, win, attribs.width, attribs.height);
  XMapWindow(wm.display, winFrame);

  memset(&wm.client_windows[wm.clients_count], 0, sizeof(Client));
  wm.client_windows[wm.clients_count].frame = winFrame;
  wm.client_windows[wm.clients_count].win = win;
  wm.client_windows[wm.clients_count].inLayout = (wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER);
  wm.client_windows[wm.clients_count].fullscreen = false;
  wm.client_windows[wm.clients_count].desktopIndex = wm.currentDesktop; 
  wm.clients_count++;

  XGrabButton(wm.display, Button1, MASTER_KEY, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.display, Button3, MASTER_KEY, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  retileLayout();
}

int main(void){
  wm = xwm_init();
  xwm_run();
  return 0;
}
