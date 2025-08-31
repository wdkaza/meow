#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>
#include "config.h"
#include <sys/select.h>

// bug where windows dont get mapped until next event
// still exists bcs of bad bar update, but its better now, TODO: fix later

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

// thanks @cococry for some of the code
// @cococry also insipered me to try to code this 
// like actually massive props to cococry's old code and dwm's code 
// it helpmed me so much to understand how to even start making a wm

#define CLIENT_WINDOW_CAP 256


// the code is a big mess overall is what i can say
// its my first time making a window manager and i didnt know alot of things
// im planning to start from scratch with the new knowledge, after finishing everything i want

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
  int value;
  time_t lastUpdate;
  bool needsUpdate;
  bool valid;
} BarModuleData;

typedef struct {
  Window win;
  bool hidden;
  char barText[256];
  XftFont *font;
  XftColor color;
  XftDraw* draw;

  BarModuleData modules[BAR_SEGMENTS_COUNT];
  time_t lastTimeUpdate;
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
static bool desktopHasWindows(int desktop);
static void initBar();
static void updateBar();
static void initBarModules();
static void hideBar();
static void forceModuleUpdate(const char* moduleName);
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

  initBarModules();

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

void grabKeys(void){
  KeyCode code;

  XUngrabKey(wm.display, AnyKey, AnyModifier, wm.root);

  for(unsigned int i = 0; i < sizeof(keys)/sizeof(*keys); i++){
    code = XKeysymToKeycode(wm.display, keys[i].key);
    if(code){
    XGrabKey(wm.display,
             code,
             keys[i].modifier,
             wm.root,
             true,
             GrabModeAsync,
             GrabModeAsync);
    }
  }
}

void xwm_run(){
  wm.running = true;

  Cursor cursor = XCreateFontCursor(wm.display, XC_left_ptr);
  XDefineCursor(wm.display, wm.root, cursor);

  XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(wm.display, false);

  grabKeys();

  wm.clients_count = 0;
  wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.windowGap = START_WINDOW_GAP;
  wm.currentLayout = WINDOW_LAYOUT_TILED_MASTER;
  wm.currentDesktop = 1;
  wm.ATOM_WM_PROTOCOLS = XInternAtom(wm.display, "WM_PROTOCOLS", false);
  wm.ATOM_WM_DELETE_WINDOW = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);

  initBar();
  initDesktops();

  int xfd = XConnectionNumber(wm.display);
  fd_set readFds;
  struct timeval timeout;
  time_t lastBarUpdate = 0;
  bool barNeedsUpdate = true;

  while(wm.running){
    FD_ZERO(&readFds);
    FD_SET(xfd, &readFds);
    
    time_t currentTime = time(NULL);
    time_t TimeSinceLastUpdate = currentTime - lastBarUpdate;

    if(TimeSinceLastUpdate >= BAR_REFRESH_TIME){
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;
      barNeedsUpdate = true;
    }
    else{
      timeout.tv_sec = BAR_REFRESH_TIME - TimeSinceLastUpdate;
      timeout.tv_usec = 0;
    }

    int ready = select(xfd + 1, &readFds, NULL, NULL, &timeout);
    
    XFlush(wm.display);

    while (XPending(wm.display)) {
      XEvent ev;
      XNextEvent(wm.display, &ev);

      switch(ev.type){
        default:
          break;
        case MapRequest:
          handleMapRequest(&ev);
          barNeedsUpdate = true;
          break;
        case UnmapNotify:
          handleUnmapNotify(&ev);
          barNeedsUpdate = true;
          break;
        case DestroyNotify:
          handleDestroyNotify(&ev);
          barNeedsUpdate = true;
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
          barNeedsUpdate = true;
          break;
        case KeyRelease:
          handleKeyRelease(&ev);
          break;
      }
    }
    if(barNeedsUpdate || ready == 0){ 
      updateBar();
      barNeedsUpdate = false;
      lastBarUpdate = time(NULL);
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

  if((e->state & MOD) && e->button == Button1){
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
  if((e->state & MOD) && (e->state & Button1Mask)){
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
  else if((e->state & MOD) && (e->state & Button3Mask)){
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

  for(uint32_t i = 0; i < wm.clients_count; i++){
    setFocusToWindow(wm.client_windows[i].win);
    return;
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
  KeySym keysym = XkbKeycodeToKeysym(wm.display, e->keycode, 0, 0);

  unsigned int cleanmask = e->state & ~(LockMask | Mod2Mask);

  for(unsigned int i = 0; i < sizeof(keys)/sizeof(*keys); i++){
    if(keys[i].key == keysym && keys[i].modifier == cleanmask){
      keys[i].func(&keys[i].arg);
    }
  }
}

void spawn(Arg *arg){
  if(fork() == 0){
    setsid();
    execvp(((char *const *)arg->v)[0], (char *const *)arg->v);
    perror("execvp");
    exit(1);
  }
}

void kill(Arg *arg){
  Window focusedWindow = wm.focused_Window;

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
        XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
      }
    }
    else{
      XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
    }
  }
}


void increaseVolume(Arg *arg){
  (void)arg;

  system(VOLUME_UP_CMD);
  forceModuleUpdate("volume");
}

void decreaseVolume(Arg *arg){
  (void)arg;
  
  system(VOLUME_DOWN_CMD);
  forceModuleUpdate("volume");
}

void muteVolume(Arg *arg){
  (void)arg;

  system(VOLUME_MUTE_CMD);
  forceModuleUpdate("volume");
}

void increaseBrightness(Arg *arg){
  (void)arg;

  system(BRIGHTNESS_UP_CMD);
  forceModuleUpdate("brightness");
}

void decreaseBrightness(Arg *arg){
  (void)arg;

  system(BRIGHTNESS_DOWN_CMD);
  forceModuleUpdate("brightness");
}

void minBrightness(Arg *arg){
  (void)arg;

  system(BRIGHTNESS_MIN_CMD);
  forceModuleUpdate("brightness");
}

void increaseGapSize(Arg *arg){
  (void)arg;

  wm.windowGap = MIN(wm.windowGap + 2, 100);
  retileLayout();
}

void decreaseGapSize(Arg *arg){
  (void)arg;

  wm.windowGap = MAX((int)wm.windowGap - 2, 15);
  retileLayout();
}

void setWindowLayoutTiled(Arg *arg){
  (void)arg;

  wm.currentLayout = WINDOW_LAYOUT_TILED_MASTER;
  for(uint32_t i = 0; i < wm.clients_count; i++){
    wm.client_windows[i].inLayout = true;
    if(wm.client_windows[i].fullscreen){
    unsetFullscreen(wm.client_windows[i].frame);
    }
  }
  retileLayout();
}

void setWindowLayoutFloating(Arg *arg){
  (void)arg;

  wm.currentLayout = WINDOW_LAYOUT_FLOATING; 
  retileLayout();
}

void addWindowToLayout(Arg *arg){
  Window focusedWindow = wm.focused_Window;

  if(clientWindowExists(focusedWindow)){
    wm.client_windows[getClientIndex(focusedWindow)].inLayout = true;
    retileLayout();
  }
}

void moveWindowUp(Arg *arg){
  Window focusedWindow = wm.focused_Window;

  moveClientUpLayout(&wm.client_windows[getClientIndex(focusedWindow)]);
}

void moveWindowDown(Arg *arg){
  Window focusedWindow = wm.focused_Window;

  moveClientDownLayout(&wm.client_windows[getClientIndex(focusedWindow)]);
}

void fullscreen(Arg *arg){
  Window focusedWindow = wm.focused_Window;

  uint32_t clientIndex = getClientIndex(focusedWindow);
  if(!wm.client_windows[clientIndex].fullscreen){
    setFullscreen(focusedWindow);
  }
  else{
    unsetFullscreen(focusedWindow);
  }
}

void enableBar(Arg *arg){
  (void)arg;

  unhideBar();
}

void disableBar(Arg *arg){
  (void)arg;

  hideBar();
}

void cycleWindows(Arg *arg){
  (void)arg;
  Window focusedWindow = wm.focused_Window;

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

void switchDesktop(Arg *arg){
  int newDesktop = arg->i;

  if(newDesktop != wm.currentDesktop && newDesktop < DESKTOP_COUNT){
    changeDesktop(newDesktop);
  }
}

void transferWindowToDesktop(Arg *arg){
  int newDesktop = arg->i;
  Window focusedWindow = wm.focused_Window;

  if(clientWindowExists(focusedWindow) && newDesktop < DESKTOP_COUNT){
    moveWindowToDesktop(focusedWindow, newDesktop);
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
  XSync(wm.display, false);
  setFocusToWindow(e->window);
}

void handleUnmapNotify(XEvent *ev){
  XUnmapEvent *e = &ev->xunmap;

  if(e->window == wm.bar.win){
    return;
  }

  uint32_t closingIndex = getClientIndex(e->window);
  if(closingIndex == (uint32_t)-1) return;


  Window focusedWindow;
  int revert_to;
  XGetInputFocus(wm.display, &focusedWindow, &revert_to);
  int32_t desktopIndex = wm.client_windows[closingIndex].desktopIndex;
  //bool was_focused = (focusedWindow == e->window);

  xwmWindowUnframe(e->window);
  focusNextWindow();
  if(!desktopHasWindows(desktopIndex)){ // a bad fix to a bug where wm would shoot itself in a leg and die
    for(int32_t i = 0; i < DESKTOP_COUNT; i++){
      if(desktopHasWindows(i)){
        changeDesktop(i);
        break;
      }
    }
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
  XMoveResizeWindow(wm.display,
                    frame,
                    wm.windowGap - BORDER_WIDTH,
                    wm.windowGap - BORDER_WIDTH,
                    screenWidth - wm.windowGap * 2,
                    screenHeight - wm.windowGap * 2);
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

int executeCommand(const char *command){
  if(!command || strlen(command) == 0) return -1;

  FILE *fp = popen(command, "r");
  if(!fp) return -1;

  char buffer[256];
  int result = -1;

  if(fgets(buffer, sizeof(buffer), fp) != NULL){
    buffer[strcspn(buffer, "\n")] = 0;
    result = atoi(buffer);
  }
  
  pclose(fp);
  return result;
}

bool updateBarModule(int moduleIndex){
  if(moduleIndex >= BAR_SEGMENTS_COUNT || !BarSegments[moduleIndex].enabled){
    return false;
  }
  
  time_t currentTime = time(NULL);
  BarModuleData *module = &wm.bar.modules[moduleIndex];

  if(module->needsUpdate || !module->valid || (currentTime - module->lastUpdate) >= BAR_REFRESH_TIME){
    int newValue = executeCommand(BarSegments[moduleIndex].command);
    if(newValue >= 0){
      module->value = newValue;
      module->lastUpdate = currentTime;
      module->needsUpdate = false;
      module->valid = true;
      return true;
    }
    else{
      module->needsUpdate = false;
      return module->valid;
    }
  }

  return module->valid;
}

void forceModuleUpdate(const char* moduleName){
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    if(strcmp(BarSegments[i].name, moduleName) == 0){
      wm.bar.modules[i].needsUpdate = true;
      break;
    }
  }
}

void initBarModules(){
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    wm.bar.modules[i].value = 0;
    wm.bar.modules[i].lastUpdate = 0;
    wm.bar.modules[i].needsUpdate = true;
    wm.bar.modules[i].valid = false;
  }
  wm.bar.lastTimeUpdate = 0;
}

void generateDesktopString(char *buffer, size_t bufferSize){
  buffer[0] = '\0';
  bool first = true;
  
  for(int i = 0; i < DESKTOP_COUNT; i++){
    if(desktopHasWindows(i)){
      if(!first){
        strncat(buffer, "-", bufferSize - strlen(buffer) - 1);
      }
      
      char tmp[8];
      if(i == wm.currentDesktop){
        snprintf(tmp, sizeof(tmp), "[%d]", i);
      }else{
        snprintf(tmp, sizeof(tmp), "%d", i);
      }
      strncat(buffer, tmp, bufferSize - strlen(buffer) - 1);
      first = false;
    }
  }
  
  if(strlen(buffer) == 0){
    snprintf(buffer, bufferSize, "[%d]", wm.currentDesktop);
  }
}

int getTextWidth(const char *text){
  if (!text || strlen(text) == 0) return 0;
  
  XGlyphInfo extents;
  XftTextExtentsUtf8(wm.display, wm.bar.font, (XftChar8*)text, strlen(text), &extents);
  return extents.xOff;
}

void drawTextAt(int x, int y, const char *text){
  if(!text || strlen(text) == 0) return;
  
  XftDrawStringUtf8(wm.bar.draw, &wm.bar.color, wm.bar.font, x, y, (XftChar8*)text, strlen(text));
}

void drawBar(){
  if(wm.bar.hidden) return;
  
  XClearWindow(wm.display, wm.bar.win);
  
  XWindowAttributes attrs;
  XGetWindowAttributes(wm.display, wm.bar.win, &attrs);
  
  int baseline = (attrs.height + wm.bar.font->ascent - wm.bar.font->descent) / 2;
  
  char segmentTexts[BAR_SEGMENTS_COUNT][256];
  int segmentWidths[BAR_SEGMENTS_COUNT];
  bool segmentValid[BAR_SEGMENTS_COUNT];
  
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    segmentTexts[i][0] = '\0';
    segmentWidths[i] = 0;
    segmentValid[i] = false;
    
    if(!BarSegments[i].enabled) continue;
    
    if(strcmp(BarSegments[i].name, "desktop") == 0){
      char desktopStr[128];
      generateDesktopString(desktopStr, sizeof(desktopStr));
      snprintf(segmentTexts[i], sizeof(segmentTexts[i]), BarSegments[i].format, desktopStr);
      segmentValid[i] = true;
    }
    else if(strcmp(BarSegments[i].name, "string") == 0){
      char stringStr[64];
      FILE *fp = popen(BarSegments[i].command, "r");
      if(fp){
        if(fgets(stringStr, sizeof(stringStr), fp) != NULL){
          stringStr[strcspn(stringStr, "\n")] = 0;
          snprintf(segmentTexts[i], sizeof(segmentTexts[i]), BarSegments[i].format, stringStr);
          segmentValid[i] = true;
        }
        pclose(fp);
      }
    }
    else if (wm.bar.modules[i].valid){
      snprintf(segmentTexts[i], sizeof(segmentTexts[i]), BarSegments[i].format, wm.bar.modules[i].value);
      segmentValid[i] = true;
    }
    
    if(segmentValid[i]){
      segmentWidths[i] = getTextWidth(segmentTexts[i]);
    }
  }
  
  
  int leftX = BAR_MODULE_PADDING;
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    if(segmentValid[i] && BarSegments[i].position == SEGMENT_LEFT){
      drawTextAt(leftX, baseline, segmentTexts[i]);
      leftX += segmentWidths[i] + BAR_SEGMENTS_COUNT;
    }
  }
  
  int totalRightWidth = 0;
  int rightCount = 0;
  
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    if(segmentValid[i] && BarSegments[i].position == SEGMENT_RIGHT){
      totalRightWidth += segmentWidths[i];
      if(rightCount > 0) totalRightWidth += BAR_SEGMENTS_COUNT;
      rightCount++;
    }
  }
  
  int rightX = attrs.width - BAR_MODULE_PADDING - totalRightWidth;
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    if(segmentValid[i] && BarSegments[i].position == SEGMENT_RIGHT){
      drawTextAt(rightX, baseline, segmentTexts[i]);
      rightX += segmentWidths[i] + BAR_SEGMENTS_COUNT;
    }
  }
  
  int totalCenterWidth = 0;
  int centerCount = 0;
  
  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    if(segmentValid[i] && BarSegments[i].position == SEGMENT_CENTER){
      totalCenterWidth += segmentWidths[i];
      if (centerCount > 0) totalCenterWidth += BAR_SEGMENTS_COUNT;
      centerCount++;
    }
  }
  
  int leftBoundary = leftX;
  int rightBoundary = attrs.width - BAR_MODULE_PADDING - totalRightWidth - BAR_SEGMENTS_COUNT;
  int availableCenterSpace = rightBoundary - leftBoundary;
  
  if(totalCenterWidth <= availableCenterSpace){
    int centerStartX = leftBoundary + (availableCenterSpace - totalCenterWidth) / 2;
    
    for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
      if(segmentValid[i] && BarSegments[i].position == SEGMENT_CENTER){
        drawTextAt(centerStartX, baseline, segmentTexts[i]);
        centerStartX += segmentWidths[i] + BAR_SEGMENTS_COUNT;
      }
    }
  } 
  else{
    int fallbackX = leftBoundary + BAR_SEGMENTS_COUNT;
    for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
      if(segmentValid[i] && BarSegments[i].position == SEGMENT_CENTER){
        if(fallbackX + segmentWidths[i] < rightBoundary){
          drawTextAt(fallbackX, baseline, segmentTexts[i]);
          fallbackX += segmentWidths[i] + BAR_SEGMENTS_COUNT;
        }
      }
    }
  }
  
  XFlush(wm.display);
}

void updateBar(){
  if(wm.bar.hidden) return;

  for(int i = 0; i < BAR_SEGMENTS_COUNT; i++){
    if(BarSegments[i].enabled && strcmp(BarSegments[i].name, "desktop") != 0){
      updateBarModule(i);
    }
  }
  drawBar();
}

bool desktopHasWindows(int desktop){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == desktop) return true;
  }
  return false;
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

void moveClientUpLayout(Client *client){
  int32_t clientIndex = getClientIndex(client->win);
  if(clientIndex == -1 || wm.clients_count <= 1) return;

  int32_t targetIndex = -1;
  for(uint32_t i = 1; i < wm.clients_count; i++){
    int32_t idx = (clientIndex + i) % wm.clients_count;
    if(wm.client_windows[idx].desktopIndex == client->desktopIndex){
      targetIndex = idx;
      break;
    }
  }
  if(targetIndex == -1) return;

  Client tmp = wm.client_windows[clientIndex];
  wm.client_windows[clientIndex] = wm.client_windows[targetIndex];
  wm.client_windows[targetIndex] = tmp;
  retileLayout();
}

void moveClientDownLayout(Client *client){
  int32_t clientIndex = getClientIndex(client->win);

  if(clientIndex == -1 || wm.clients_count <= 1) return;

  int32_t targetIndex = -1;
  for(uint32_t i = 1; i < wm.clients_count; i++){
    int32_t idx = (clientIndex - (int32_t)i + (int32_t)wm.clients_count) % (int32_t)wm.clients_count;
    if(wm.client_windows[idx].desktopIndex == client->desktopIndex){
      targetIndex = idx;
      break;
    }
  }
  if(targetIndex == -1) return;

  Client tmp = wm.client_windows[clientIndex];
  wm.client_windows[clientIndex] = wm.client_windows[targetIndex];
  wm.client_windows[targetIndex] = tmp;
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

  XGrabButton(wm.display, Button1, MOD, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.display, Button3, MOD, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  retileLayout();
}

int main(void){
  wm = xwm_init();
  xwm_run();
  return 0;
}
