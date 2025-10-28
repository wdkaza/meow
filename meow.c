#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>
#include <X11/Xresource.h>
#include <stdbool.h>
#include "config.h"
#include "structs.h"
#include <sys/select.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

// hello! thanks for checking out this code, overall its a mess is what i can say
// it would be nice to rewrite it someday but im happy with it for now
// and aslong as everything you see works fine its ok :D

// this project started with a "what if" question :) so please go ahead and do something you always wanted

#define CLIENT_WINDOW_CAP 256

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) ((a) > (b) ? (a) : b)
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define EVENT_SIZE		(sizeof(struct inotify_event))
#define BUF_LEN			(1024 * (EVENT_SIZE + 16))

static unsigned int numlockmask = 0;
#define MODMASK(mask) (mask & ~(numlockmask | LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define LENGTH(X)     (sizeof X / sizeof X[0])

typedef enum{
  WINDOW_LAYOUT_TILED_MASTER = 0,
  WINDOW_LAYOUT_TILED_CASCADE,
  WINDOW_LAYOUT_HORIZONTAL_MASTER,
  WINDOW_LAYOUT_FLOATING
} WindowLayout;

typedef enum{
  RESIZE_NONE = 0,
  RESIZE_BOTTOM_RIGHT,
  RESIZE_BOTTOM_LEFT,
  RESIZE_TOP_RIGHT,
  RESIZE_TOP_LEFT,
} ResizeMode;

typedef struct {
  float x, y;
} Vec2;

typedef struct{
  int borderWidth;
  int borderFocused;
  int borderUnfocused;
  uint32_t windowGap;
  uint32_t masterWindowGap;
  int topBorder;
  int bottomBorder;
  bool masterLeft;
} Configuration;


typedef struct {
  Window win;
  Window frame;
  bool fullscreen;
  Vec2 fullscreenRevertSize;
  bool inLayout;
  bool isMaster;
  int heightInLayout;
  bool forceManage;
  ResizeMode resizeMode;
  int32_t desktopIndex;
} Client;

typedef struct {
  Display* display;
  Window root;

  bool running;

  Client client_windows[CLIENT_WINDOW_CAP];
  uint32_t clients_count;
  int32_t currentDesktop;

  bool mouseHeldDown;
  Vec2 cursor_start_pos;
  Vec2 cursor_start_frame_pos;
  Vec2 cursor_start_frame_size;

  WindowLayout currentLayout;
  Window focused_Window;

  int screenHeight;
  int screenWidth;

  Configuration conf;

  Atom ATOM_WM_DELETE_WINDOW;
  Atom ATOM_WM_PROTOCOLS;
  Atom ATOM_NET_ACTIVE_WINDOW;
} XWM;

static void handleConfigureRequst(XEvent *ev);
static void handleUnmapNotify(XEvent *ev);
static void handleDestroyNotify(XEvent *ev);
static void handleConfigureNotify(XEvent *ev);
static void handleMapRequest(XEvent *ev);
static void xwmWindowFrame(Window win);
static void xwmWindowUnframe(Window w);
static void handleButtonPress(XEvent *ev);
static void handleKeyRelease(XEvent *ev);
static void handleButtonRelease();
static void handleMotionNotify(XEvent *ev);
static void handleKeyPress(XEvent *ev);
static void handleEnterNotify(XEvent *ev);
static void handleClientMessage(XEvent *ev);
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
static void dwm_updateNumlockMask();
static void dwm_grabKeys(void);
static Client *getClientFromWindow(Window win);
static XWM xwm_init();
static void xwm_run();
static void sendConfigureNotify(Client *c);
static void focusNextWindow();
static void recalculateFixedPartialStrut();
static void updateWindowBorders(Window w);
static void applyRules(Client *c);
static void updateDesktopProperties();
static bool isLayoutTiling(WindowLayout layout);
static void FGetFixedPartialStrut(Window w);
static void updateActiveWindow(Window w);
static Client *getFocusedSlaveClient();
static void swapMasterWithSlave(Client *client);
static void moveSlavesStackUp(Client *client);
static void moveSlavesStackDown(Client *client);
static void fixFocusForCascade();
static void loadXResources();
static void reloadXresources();
static void loadConfig();
static void *inotifyXresources(void *arg);
static void reloadWindows();
static void polybarLayoutIPC(int num);
static XWM wm;

void sendConfigureNotify(Client *c){ // i hate this function
  XConfigureEvent ev;
  ev.type = ConfigureNotify;
  ev.display = wm.display;
  ev.event = c->win;
  ev.window = c->win; 

  XWindowAttributes wa;
  XGetWindowAttributes(wm.display, c->frame, &wa);
  
  ev.x = wa.x;
  ev.y = wa.y;
  ev.width = wa.width;
  ev.height = wa.height;
  ev.border_width = 0;
  ev.above = None;
  XSendEvent(wm.display, c->win, false, StructureNotifyMask, (XEvent *)&ev);
}

void reloadXresources(){
  char prog[255] = "xrdb ~/.Xresources";
  if(system(prog) == -1){
    printf("cant start %s\n", prog);
  }
  loadXResources();
  reloadWindows();
}

void loadXResources(){ // massive thanks to FluoriteWM for XResources pieces of code
  char *xrm;
  char *type;
  XrmDatabase xdb;
  XrmValue xval;
  Display *dummyDisplay;

  loadConfig();

  dummyDisplay = XOpenDisplay(NULL);
  xrm = XResourceManagerString(dummyDisplay);
  if(!xrm) return;

  xdb = XrmGetStringDatabase(xrm);

  if(XrmGetResource(xdb, "wm.borderWidth", "*", &type, &xval)){
    if(xval.addr){
      wm.conf.borderWidth = strtoul(xval.addr, NULL, 0);
    }
  }
  if(XrmGetResource(xdb, "wm.borderFocused", "*", &type, &xval)){
    if(xval.addr){
      char *addr = xval.addr;
      if(*addr == '#') addr++;
      wm.conf.borderFocused = strtoul(addr, NULL, 16);
    }
  }
  if(XrmGetResource(xdb, "wm.borderUnfocused", "*", &type, &xval)){
    if(xval.addr){
      char *addr = xval.addr;
      if(*addr == '#') addr++;
      wm.conf.borderUnfocused = strtoul(addr, NULL, 16);
    }
  }
  if(XrmGetResource(xdb, "wm.windowGap", "*", &type, &xval)){
    if(xval.addr){
      wm.conf.windowGap = strtoul(xval.addr, NULL, 0);
    }
  }
  wm.conf.borderFocused |= 0xff << 24;
  wm.conf.borderUnfocused |= 0xff << 24;
  XrmDestroyDatabase(xdb);
  free(xrm);
}

void *inotifyXresources(void *arg){ // massive thanks to FluoriteWM for XResources pieces of code
  (void)arg;
  const char *filename = ".Xresources";
  char *home = getenv("HOME");
  char buf[BUF_LEN];
  int fd;

  if((fd = inotify_init1(IN_NONBLOCK)) < 0){
    return NULL;
  }
  inotify_add_watch(fd, home, IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO); 

  while(1){
    ssize_t len = read(fd, buf, BUF_LEN);
    if(len < 0){
      if(errno == EAGAIN || errno == EWOULDBLOCK){
        usleep(100000);
        continue;
      }
      break;
    }
    ssize_t i = 0;
    while(i < len){
      struct inotify_event *event = (struct inotify_event *) &buf[i];
      if(event->len > 0 && strcmp(event->name, filename) == 0){
        reloadXresources();
      }
      i += EVENT_SIZE + event->len;
    }
  }

  close(fd);
  return NULL;
}

void loadConfig(){
  wm.conf.borderFocused = BORDER_FOCUSED_COLOR;
  wm.conf.borderUnfocused = BORDER_COLOR;
  wm.conf.borderWidth = BORDER_WIDTH;
  wm.conf.windowGap = START_WINDOW_GAP;
}

void reloadWindows(){
  if(wm.clients_count == 0){
    return;
  }

  for(uint32_t i = 0; i < wm.clients_count; i++){
    Client *client = &wm.client_windows[i];
    if(client->desktopIndex == wm.currentDesktop && client->fullscreen){
      return;
    }
  }
  if(isLayoutTiling(wm.currentLayout)){
    retileLayout();
    XRaiseWindow(wm.display, wm.client_windows[0].frame);
  }
  for(uint32_t i = 0; i < wm.clients_count; i++){
    Client *client = &wm.client_windows[i];
    if(client->desktopIndex != wm.currentDesktop){
      continue;
    }
    int borderColor = (client->win == wm.focused_Window) ? wm.conf.borderFocused : wm.conf.borderUnfocused;
    XSetWindowBorder(wm.display, client->frame, borderColor);
    XSetWindowBorderWidth(wm.display, client->frame, wm.conf.borderWidth);
  }
  if(wm.focused_Window != None){
    XSetInputFocus(wm.display, wm.focused_Window, RevertToPointerRoot, CurrentTime);
    XChangeProperty(wm.display, wm.root, wm.ATOM_NET_ACTIVE_WINDOW, XA_WINDOW, 32, PropModeReplace, (const unsigned char *)&wm.focused_Window, 1);
  }
  XFlush(wm.display);
}

int xwm_error_handler(Display *dpy, XErrorEvent *e){
  if(e->error_code == BadWindow || e->error_code == BadDrawable || e->error_code == BadMatch){
    return 0;
  }

  char buf[1024];
  XGetErrorText(dpy, e->error_code, buf, sizeof(buf));
  fprintf(stderr, "x error: %s (request %d, resource: %lu)\n", buf, e->error_code, e->resourceid);
  return 0;
}

void polybarLayoutIPC(int num){
  char command[256];
  snprintf(command, sizeof(command), "polybar-msg action \"#meow_layout.hook.%d\"", num);
  strcat(command, " &");
  if(system(command) == -1) printf("failed to execute command/program\n");
}

void initDesktops(){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex < 0 || wm.client_windows[i].desktopIndex >= DESKTOP_COUNT){
      wm.client_windows[i].desktopIndex = 0;
    }
  }
}

ResizeMode getResizeMode(Window frame, int x, int y){
  XWindowAttributes attr;
  XGetWindowAttributes(wm.display, frame, &attr);

  int mid_x = attr.width / 2;
  int mid_y = attr.height / 2;

  bool left = x <= mid_x;
  bool right = x >= mid_x;
  bool top = y <= mid_y;
  bool bottom = y >= mid_y;
    
  if(top && left) return RESIZE_TOP_LEFT;
  if(top && right) return RESIZE_TOP_RIGHT;
  if(bottom && left) return RESIZE_BOTTOM_LEFT;
  if(bottom && right) return RESIZE_BOTTOM_RIGHT;

  return RESIZE_NONE; // shouldnt be reached ever but fallback just incase
}

Window getToplevel(Window w){ // dont know if i will use this fucntion yet
	while(1){
		Window root, parent;
		Window *children;
		unsigned int nchildren;
		if(!XQueryTree(wm.display, w, &root, &parent, &children, &nchildren) || parent == root || parent == 0) break;
		w = parent;
		if(children) XFree(children);
	}
	return w;
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
  XrmInitialize();
  loadXResources();

  XSetWindowAttributes attrs;
  attrs.backing_store = Always;
  XChangeWindowAttributes(wm.display, wm.root, CWBackingStore, &attrs);
  XSetCloseDownMode(wm.display, RetainPermanent);
  XSync(wm.display, false);

  Window wmWin = XCreateSimpleWindow(
    wm.display,
    wm.root,
    0,0,1,1,0,
    BlackPixel(wm.display, screen),
    BlackPixel(wm.display, screen)
  );

  Atom netSupportingWmCheck = XInternAtom(wm.display, "_NET_SUPPORTING_WM_CHECK", false);
  Atom netWmName =            XInternAtom(wm.display, "_NET_WM_NAME", false);
  Atom utf8Str =              XInternAtom(wm.display, "UTF8_STRING", false);

  XChangeProperty(wm.display, wm.root, netSupportingWmCheck, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmWin, 1);
  XChangeProperty(wm.display, wmWin, netSupportingWmCheck, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmWin, 1);

  char *wmName = "meow";
  XChangeProperty(wm.display, wmWin, netWmName, utf8Str, 8, PropModeReplace, (unsigned char *)wmName, strlen(wmName));


  return wm;
}

void dwm_updateNumlockMask(){
  unsigned int i;
  int j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(wm.display);
  for(i = 0; i < 8; i++){
    for(j = 0; j < modmap->max_keypermod; j++){
      if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(wm.display, XK_Num_Lock)){
        numlockmask = (1 << i);
      }
    }
  }
  XFreeModifiermap(modmap);
}

void dwm_grabKeys(void)
{
  dwm_updateNumlockMask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask|LockMask};
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(wm.display, AnyKey, AnyModifier, wm.root);
    XDisplayKeycodes(wm.display, &start, &end);
    syms = XGetKeyboardMapping(wm.display, start, end - start + 1, &skip);
    if(!syms){
      return;
    }
    for(k = start; k <= (unsigned int)end; k++){
      for(i = 0; i < LENGTH(keys); i++){
        if(keys[i].key == syms[(k - start) * skip]){
          for(j = 0; j < LENGTH(modifiers); j++){
            XGrabKey(wm.display, k, keys[i].modifier | modifiers[j], wm.root, True, GrabModeAsync, GrabModeAsync);
          }
        }
      }
    }
    XFree(syms);
  }
}

void xwm_run(){
  // TOOD : some code should be moved out of run to init, because wth is this mess
  wm.running = true;

  Cursor cursor = XCreateFontCursor(wm.display, XC_left_ptr);
  XDefineCursor(wm.display, wm.root, cursor);

  XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(wm.display, false);

  dwm_grabKeys();

  wm.clients_count = 0;
  wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.currentLayout = WINDOW_LAYOUT_TILED_MASTER;
  wm.currentDesktop = 1;
  wm.ATOM_WM_PROTOCOLS = XInternAtom(wm.display, "WM_PROTOCOLS", false);
  wm.ATOM_WM_DELETE_WINDOW = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
  wm.ATOM_NET_ACTIVE_WINDOW = XInternAtom(wm.display, "_NET_ACTIVE_WINDOW", false);

  wm.conf.windowGap = START_WINDOW_GAP;
  wm.conf.masterWindowGap = 0;
  wm.conf.topBorder = 0;
  wm.conf.bottomBorder = 0;
  wm.conf.masterLeft = true;

  reloadXresources();


  updateActiveWindow(None);
  updateDesktopProperties();

  initDesktops();

  int xfd = XConnectionNumber(wm.display);
  fd_set readFds;
  struct timeval timeout;
  time_t lastUpdate = 0;
  bool needsUpdate = true;

  while(wm.running){
    FD_ZERO(&readFds);
    FD_SET(xfd, &readFds);

    time_t currentTime = time(NULL);
    time_t timeSinceLastUpdate = currentTime - lastUpdate;

    if(timeSinceLastUpdate >= REFRESH_TIME){
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;
      needsUpdate = true;
    }
    else{
      timeout.tv_sec = REFRESH_TIME - timeSinceLastUpdate;
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
          needsUpdate = true;
          break;
        case UnmapNotify:
          handleUnmapNotify(&ev);
          needsUpdate = true;
          break;
        case DestroyNotify:
          handleDestroyNotify(&ev);
          needsUpdate = true;
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
        case ButtonRelease:
          handleButtonRelease();
          break; 
        case MotionNotify:
          handleMotionNotify(&ev);
          break;
        case EnterNotify:
          handleEnterNotify(&ev);
          needsUpdate = true;
          break;
        case KeyPress:
          handleKeyPress(&ev);
          needsUpdate = true;
          break;
        case KeyRelease:
          handleKeyRelease(&ev);
          break;
        case ClientMessage:
				  handleClientMessage(&ev);
				  break;
      }
    }
    if(needsUpdate || ready == 0){
      needsUpdate = false;
      lastUpdate = time(NULL);
    }
  }
}

void handleEnterNotify(XEvent *ev){
  if(FOCUS_WINDOWS_UNDER_MOUSE){
    XCrossingEvent *e = &ev->xcrossing; 
    if(wm.mouseHeldDown) return;
    if(e->window == wm.root) return;
    if(e->mode != NotifyNormal) return;

    Client *client = getClientFromWindow(e->window);
    if(client){
      if(wm.focused_Window == client->win) return;
      if(AUTOMATICLY_RAISE_WINDOWS_WITH_MOUSE){
        setFocusToWindow(client->win);
      }
      else{
        XSetInputFocus(wm.display, client->win, RevertToParent, CurrentTime);
        updateWindowBorders(client->win);
        updateActiveWindow(client->win);
      }
      return;
    }
    //handle childern
    Window frame = getFrameWindow(e->window);
    if(frame != None){
      int32_t id = getClientIndex(frame);
      if(id >= 0 && (uint32_t)id < wm.clients_count){
        Client *clientChild = &wm.client_windows[id];
        if(wm.focused_Window == clientChild->win) return;
        if(AUTOMATICLY_RAISE_WINDOWS_WITH_MOUSE){
          setFocusToWindow(client->win);
        }
        else{
          XSetInputFocus(wm.display, client->win, RevertToParent, CurrentTime);
          updateWindowBorders(client->win);
          updateActiveWindow(client->win);
        }
        return;
      }
    }
  }
  else{
    return;
  }
}

void handleClientMessage(XEvent *ev){
  if(ev->xclient.message_type == XInternAtom(wm.display, "_NET_CURRENT_DESKTOP", False)){
    changeDesktop((int)ev->xclient.data.l[0] + 1); 
  }
  else if(ev->xclient.message_type == wm.ATOM_NET_ACTIVE_WINDOW){
    Window w = ev->xclient.window;
    if(clientWindowExists(w)){
      int32_t clientIndex = getClientIndex(w);
      if(clientIndex != -1){
        int32_t windowDesktop = wm.client_windows[clientIndex].desktopIndex;
        if(windowDesktop != wm.currentDesktop){
          changeDesktop(windowDesktop);
        }
        setFocusToWindow(w);
      }
    }
  }
  else if(ev->xclient.message_type == XInternAtom(wm.display, "_NET_WM_STATE", False)){
    Window w = ev->xclient.window;
    Atom aStateFs = XInternAtom(wm.display, "_NET_WM_STATE_FULLSCREEN", False);
    
    if(ev->xclient.data.l[1] == (long)aStateFs || ev->xclient.data.l[2] == (long)aStateFs){
      int32_t clientIndex = getClientIndex(w);
      if(clientIndex != -1){
        int action = ev->xclient.data.l[0];
        if(action == 1 || (action == 2 && !wm.client_windows[clientIndex].fullscreen)){
          setFullscreen(w);
        }
        else if(action == 0 || (action == 2 && wm.client_windows[clientIndex].fullscreen)){
          unsetFullscreen(w);
        }
      }
    }
  }
}

void updateClientList(){
  Window list[CLIENT_WINDOW_CAP];
  int listIdx = 0;

  for(int i = 0; i < CLIENT_WINDOW_CAP; i++){
    Client *c = &wm.client_windows[i];
    if(c->win != None){
      list[listIdx++] = c->win;
    }
  }

  Atom netClientList = XInternAtom(wm.display, "_NET_CLIENT_LIST", false);

  XChangeProperty(wm.display, wm.root, netClientList, XA_WINDOW, 32, PropModeReplace, (unsigned char *)list, listIdx);

}

void updateDesktopProperties(){

  Atom netNumberOfDesktops = XInternAtom(wm.display, "_NET_NUMBER_OF_DESKTOPS", False);
  long desktopCount = DESKTOP_COUNT - 1;
  XChangeProperty(wm.display, wm.root, netNumberOfDesktops, XA_CARDINAL, 32, 
    PropModeReplace, (unsigned char *)&desktopCount, 1);

  Atom netCurrentDesktop = XInternAtom(wm.display, "_NET_CURRENT_DESKTOP", False);
  long currentDesktop = wm.currentDesktop - 1;
  XChangeProperty(wm.display, wm.root, netCurrentDesktop, XA_CARDINAL, 32,
    PropModeReplace, (unsigned char *)&currentDesktop, 1);

  Atom supportedAtoms[] = {
    XInternAtom(wm.display, "_NET_SUPPORTING_WM_CHECK", false),
    XInternAtom(wm.display, "_NET_WM_NAME", false),
    XInternAtom(wm.display, "_NET_NUMBER_OF_DESKTOPS", false),
    XInternAtom(wm.display, "_NET_CURRENT_DESKTOP", false),
    XInternAtom(wm.display, "_NET_CLIENT_LIST" , false),
    XInternAtom(wm.display, "_NET_WM_DESKTOP", false),
    wm.ATOM_NET_ACTIVE_WINDOW
  };

  Atom netSupported = XInternAtom(wm.display, "_NET_SUPPORTED", false);
  XChangeProperty(wm.display, wm.root, netSupported, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)supportedAtoms, sizeof(supportedAtoms) / sizeof(Atom));

}
void handleButtonPress(XEvent *ev){
  XButtonEvent *e = &ev->xbutton;

  wm.mouseHeldDown = true;

  if(!clientWindowExists(e->window)) return;

  Window targetWindow = e->window;

  if(clientWindowExists(targetWindow) || clientFrameExists(e->window)){
    setFocusToWindow(targetWindow);
  }

  Window frame = getFrameWindow(e->window);

  if((e->state & MOD) && e->button == Button1){
    wm.cursor_start_pos = (Vec2){
      .x = (float)e->x_root,
      .y = (float)e->y_root
    };

    XWindowAttributes attr;
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
  if((e->state & MOD) && e->button == Button3){
    Client *client = &wm.client_windows[getClientIndex(e->window)];
    client->resizeMode = getResizeMode(frame, e->x, e->y);
    XWindowAttributes attr;
    XGetWindowAttributes(wm.display, frame, &attr);
    wm.cursor_start_pos.x = e->x_root;
    wm.cursor_start_pos.y = e->y_root;
    wm.cursor_start_frame_pos.x = attr.x;
    wm.cursor_start_frame_pos.y = attr.y;
    wm.cursor_start_frame_size.x = attr.width;
    wm.cursor_start_frame_size.y = attr.height;
  }
}

Client *getClientFromWindow(Window win){
  for(uint32_t i = 0; i < wm.clients_count; i++){
    Client *client = &wm.client_windows[i];
    if(client->win == win || client->frame == win){
      return client;
    }
  }
  return NULL;
}

void handleButtonRelease(){
  if(wm.mouseHeldDown){
    wm.mouseHeldDown = false;
    sendConfigureNotify(getClientFromWindow(wm.focused_Window));
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
    int newX = (int)(wm.cursor_start_frame_pos.x) + dx;
    int newY = (int)(wm.cursor_start_frame_pos.y) + dy;
    
    XWindowAttributes attr;
    XGetWindowAttributes(wm.display, frame, &attr);
    
    newX = MAX(0, MIN(newX, wm.screenWidth - attr.width));
    newY = MAX(0, MIN(newY, wm.screenHeight - attr.height));
    
    if(CLAMP_FLOATING_WINDOWS){
      XMoveWindow(wm.display, frame, newX, newY);
    }
    else{
      XMoveWindow(wm.display, frame, 
                  (int)(wm.cursor_start_frame_pos.x) + dx,
                  (int)(wm.cursor_start_frame_pos.y) + dy);
    }
    
    if(client->fullscreen){
      client->fullscreen = false;
      XSetWindowBorderWidth(wm.display, client->frame, wm.conf.borderWidth);
    }
    if(client->inLayout){
      client->inLayout = false;
      retileLayout();
    }
  }
  else if((e->state & MOD) && (e->state & Button3Mask)){
    int new_x = (int)wm.cursor_start_frame_pos.x;
    int new_y = (int)wm.cursor_start_frame_pos.y;
    int new_width = (int)wm.cursor_start_frame_size.x;
    int new_height = (int)wm.cursor_start_frame_size.y;
    
    switch(client->resizeMode){
      case RESIZE_BOTTOM_RIGHT:
        new_width += dx;
        new_height += dy;
        break; 
      case RESIZE_BOTTOM_LEFT:
        new_x += dx;
        new_width -= dx;
        new_height += dy;
        break;  
      case RESIZE_TOP_RIGHT:
        new_y += dy;
        new_width += dx;
        new_height -= dy;
        break;
      case RESIZE_TOP_LEFT:
        new_x += dx;
        new_y += dy;
        new_width -= dx;
        new_height -= dy;
        break;
      default:
        new_width += dx;
        new_height += dy;
        break;
    }
    new_width = MAX(100, new_width);
    new_height = MAX(100, new_height);
    
    XMoveResizeWindow(wm.display, frame, new_x, new_y, new_width, new_height);
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
      XSetWindowBorder(wm.display, wm.client_windows[i].frame, wm.conf.borderUnfocused);
      XSetWindowBorderWidth(wm.display, wm.client_windows[i].frame, wm.conf.borderWidth);
    }
  }

  if(w != None && clientWindowExists(w)){
    Window frame = getFrameWindow(w);
    if(frame != None){
      XSetWindowBorder(wm.display, frame, wm.conf.borderFocused);
      XSetWindowBorderWidth(wm.display, frame, wm.conf.borderWidth);
    }
  }

  wm.focused_Window = w;
  XSync(wm.display, false);

}

void updateActiveWindow(Window w){
  XChangeProperty(wm.display, wm.root, wm.ATOM_NET_ACTIVE_WINDOW,
                  XA_WINDOW, 32, PropModeReplace, 
                  (unsigned char *)&w, 1);
  XSync(wm.display, false);
}

void setFocusToWindow(Window w){
  if(w != None && clientWindowExists(w)){
    XRaiseWindow(wm.display, getFrameWindow(w));
    XSetInputFocus(wm.display, w, RevertToParent, CurrentTime);
    updateWindowBorders(w);
    updateActiveWindow(w);
  }
  else{
    XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
    updateWindowBorders(None);
    updateActiveWindow(None);
  }
}

void toggleMasterSide(Arg *arg){
  (void)arg;
  wm.conf.masterLeft = !wm.conf.masterLeft;
  retileLayout();
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
  changes.border_width = e->border_width;
  changes.sibling = e->above;
  changes.stack_mode = e->detail;

  int32_t index = getClientIndex(e->window); // im pretty sure something is wrong with this function
  if(index != -1){
    Window frame = wm.client_windows[index].frame;
    if(frame != 0){
      XConfigureWindow(wm.display, frame, e->value_mask, &changes);
    }
    else{
      XConfigureWindow(wm.display, e->window, e->value_mask, &changes);
    }
  }
  else{
    XConfigureWindow(wm.display, e->window, e->value_mask, &changes);
  }
  retileLayout();
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

  unsigned int cleanmask = MODMASK(e->state);

  for(unsigned int i = 0; i < LENGTH(keys); i++){
    if(keys[i].key == keysym && keys[i].modifier == cleanmask){
      keys[i].func(&keys[i].arg);
      break;
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
  (void)arg;
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
}

void decreaseVolume(Arg *arg){
  (void)arg;
  
  system(VOLUME_DOWN_CMD);
}

void muteVolume(Arg *arg){
  (void)arg;

  system(VOLUME_MUTE_CMD);
}

void increaseBrightness(Arg *arg){
  (void)arg;

  system(BRIGHTNESS_UP_CMD);
}

void decreaseBrightness(Arg *arg){
  (void)arg;

  system(BRIGHTNESS_DOWN_CMD);
}

void minBrightness(Arg *arg){
  (void)arg;

  system(BRIGHTNESS_MIN_CMD);
}

void increaseGapSize(Arg *arg){

  wm.conf.windowGap = CLAMP((int)wm.conf.windowGap - arg->i, 15, 100);
  retileLayout();

}

void decreaseGapSize(Arg *arg){

  wm.conf.windowGap = CLAMP((int)wm.conf.windowGap + arg->i, 15, 100);
  retileLayout();

}

void increaseMasterGapSize(Arg *arg){

  wm.conf.masterWindowGap = CLAMP((int)wm.conf.masterWindowGap - arg->i, -450, 450);
  retileLayout();

}

void decreaseMasterGapSize(Arg *arg){

  wm.conf.masterWindowGap = CLAMP((int)wm.conf.masterWindowGap + arg->i, -450, 450); 
  retileLayout();

}

void resetMasterGapSize(Arg *arg){
  (void)arg;

  wm.conf.masterWindowGap = 0;
  retileLayout();

}

void increaseSlaveHeight(Arg *arg){
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER){
    Client *slave = getFocusedSlaveClient();
    if(!slave) return;

    slave->heightInLayout = CLAMP((int)slave->heightInLayout - arg->i, -300, 300);
    retileLayout();
  }
}

void decreaseSlaveHeight(Arg *arg){
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER){
    Client *slave = getFocusedSlaveClient();
    if(!slave) return;

    slave->heightInLayout = CLAMP((int)slave->heightInLayout + arg->i, -300, 300);
    retileLayout();
  }
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

void setWindowLayoutHorizontal(Arg *arg){
  (void)arg;
  wm.currentLayout = WINDOW_LAYOUT_HORIZONTAL_MASTER;
  for(uint32_t i = 0; i < wm.clients_count; i++){
    wm.client_windows[i].inLayout = true;
    if(wm.client_windows[i].fullscreen){
      unsetFullscreen(wm.client_windows[i].frame);
    }
  }
  retileLayout();
}

void setWindowLayoutCascase(Arg *arg){
  (void)arg;

  wm.currentLayout = WINDOW_LAYOUT_TILED_CASCADE;
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
  (void)arg;
  Window focusedWindow = wm.focused_Window;

  if(clientWindowExists(focusedWindow)){
    wm.client_windows[getClientIndex(focusedWindow)].inLayout = true;
    retileLayout();
  }
}

void moveWindowDown(Arg *arg){ // TODO : lmaoo these are inverted 
  (void)arg;
  Window focusedWindow = wm.focused_Window;

  moveClientUpLayout(&wm.client_windows[getClientIndex(focusedWindow)]);
}

void moveWindowUp(Arg *arg){ // TODO : lmaoo these are inverted 
  (void)arg;
  Window focusedWindow = wm.focused_Window;

  moveClientDownLayout(&wm.client_windows[getClientIndex(focusedWindow)]);
}


void swapSlaveWithMaster(Arg *arg){
  (void) arg;
  Window focusedWindow = wm.focused_Window;

  swapMasterWithSlave(&wm.client_windows[getClientIndex(focusedWindow)]);
}

void moveSlavesStackForward(Arg *arg){
  (void)arg;
  Window focusedWindow = wm.focused_Window;

  moveSlavesStackUp(&wm.client_windows[getClientIndex(focusedWindow)]);
}

void moveSlavesStackBack(Arg *arg){
  (void)arg;
  Window focusedWindow = wm.focused_Window;

  moveSlavesStackDown(&wm.client_windows[getClientIndex(focusedWindow)]);
}

void fullscreen(Arg *arg){
  (void)arg;
  Window focusedWindow = wm.focused_Window;

  uint32_t clientIndex = getClientIndex(focusedWindow);
  if(!wm.client_windows[clientIndex].fullscreen){
    setFullscreen(focusedWindow);
  }
  else{
    unsetFullscreen(focusedWindow);
  }
}

void tiledAndFloatingHelper(bool Backwards){
  Window focusedWindow = wm.focused_Window;

  int32_t currentIndex = -1;
  for(uint32_t i = 0; i < wm.clients_count; i++){
  if((wm.client_windows[i].win == focusedWindow || wm.client_windows[i].frame == focusedWindow) && wm.client_windows[i].desktopIndex == wm.currentDesktop){
    currentIndex = i;
    break;
    }
  }

  retileLayout(); // avoids weird looking fullscreen, kind of a half-ass bug fix

  if(currentIndex != -1){
    for(uint32_t i = 1; i < wm.clients_count; i++){
      uint32_t Index;
      if(Backwards){
        Index = (currentIndex - i + wm.clients_count) % wm.clients_count;
      }
      else{
        Index = (currentIndex + i) % wm.clients_count;
      }
      if(wm.client_windows[Index].desktopIndex == wm.currentDesktop){
        setFocusToWindow(wm.client_windows[Index].win);
        break;
      }
    }
  } 
}

void cascadeHelper(){
  Window focusedWindow = wm.focused_Window;

  int32_t currentIndex = -1;
  for(uint32_t i = 0; i < wm.clients_count; i++){
  if((wm.client_windows[i].win == focusedWindow || wm.client_windows[i].frame == focusedWindow) && wm.client_windows[i].desktopIndex == wm.currentDesktop){
    currentIndex = i;
    break;
    }
  }
  fixFocusForCascade();
  retileLayout();
  int32_t masterIndex = -1;
  int32_t lastClientIndex = -1;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
      if(masterIndex == -1){
        masterIndex = i;
      }
      lastClientIndex = i;
    }
  }

  int32_t cycle[wm.clients_count];
  uint32_t cycleCount = 0;

  if(masterIndex != -1) cycle[cycleCount++] = masterIndex;
  if(lastClientIndex != -1 && lastClientIndex != masterIndex) cycle[cycleCount++] = lastClientIndex;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && !wm.client_windows[i].inLayout){
      cycle[cycleCount++] = i;
    }
  }

  bool found = false;
  for(uint32_t i = 0; i < cycleCount; i++){
    if(cycle[i] == currentIndex){
      uint32_t next = (i + 1) % cycleCount;
      setFocusToWindow(wm.client_windows[cycle[next]].win);
      found = true;
    }
  }
  if(!found){
    for(uint32_t i = 0; i < wm.clients_count; i++){
      if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
        setFocusToWindow(wm.client_windows[i].win);
      }
    }
    setFocusToWindow(wm.client_windows[cycle[0]].win);
  }
}

void cycleWindows(Arg *arg){
  (void)arg;

  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER || wm.currentLayout == WINDOW_LAYOUT_HORIZONTAL_MASTER || wm.currentLayout == WINDOW_LAYOUT_FLOATING){
    tiledAndFloatingHelper(false);
  }
  else if(wm.currentLayout == WINDOW_LAYOUT_TILED_CASCADE || wm.currentLayout == WINDOW_LAYOUT_FLOATING){
    cascadeHelper();
  } 
}

void cycleWindowsBackwards(Arg *arg){
  (void)arg;

  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER || wm.currentLayout == WINDOW_LAYOUT_HORIZONTAL_MASTER || wm.currentLayout == WINDOW_LAYOUT_FLOATING){
    tiledAndFloatingHelper(true);
  }
  else if(wm.currentLayout == WINDOW_LAYOUT_TILED_CASCADE || wm.currentLayout == WINDOW_LAYOUT_FLOATING){
    cascadeHelper();
  }
}

void exitWM(Arg *arg){
  (void)arg;

  wm.running = 0;
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

// from fluorite
void FGetFixedPartialStrut(Window w){
  Atom strut_atom = XInternAtom(wm.display, "_NET_WM_STRUT_PARTIAL", false);
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;

  if(XGetWindowProperty(wm.display, w, strut_atom,
                        0, 12, false, XA_CARDINAL,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop) == Success && prop)
  {
    if(actual_type == XA_CARDINAL && actual_format == 32 && nitems >= 4){
      long *values = (long *)prop;
      if(values[2] > 0){
        wm.conf.topBorder = (int)values[2];
      }
      else if(values[3] > 0){
        wm.conf.bottomBorder = (int)values[3];
      }
    }
    XFree(prop);
  }
}

void recalculateFixedPartialStrut(){
  Window rootReturn, parentReturn;
  Window *children = NULL;
  unsigned int nchildren = 0;

  wm.conf.topBorder = 0;
  wm.conf.bottomBorder = 0;

  if(!XQueryTree(wm.display, wm.root, &rootReturn, &parentReturn, &children, &nchildren)){
    return;
  }

  for(unsigned int i = 0; i < nchildren; i++){
    Window w = children[i];
    XWindowAttributes attr;
    if(XGetWindowAttributes(wm.display, w, &attr)){
      if(attr.map_state != IsUnmapped){
        FGetFixedPartialStrut(w);
      }
    }
  }
  if(children) XFree(children);
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

void manageFloatingWindow(Client *c){
  if(!c) return;

  XWindowAttributes attr;
  if(!XGetWindowAttributes(wm.display, c->win, &attr)) return;

  unsigned int ww = (unsigned int)attr.width;
  unsigned int wh = (unsigned int)attr.height;

  int availableHeight = wm.screenHeight + wm.conf.topBorder; // TODO add bottomBorder via "?" (if its zero, etc) 

  int centerX = (wm.screenWidth - (int)ww) / 2;
  int centerY = (availableHeight - (int)wh) / 2;

  centerX -= wm.conf.borderWidth;
  centerY -= wm.conf.borderWidth;

  if(centerX < - wm.conf.borderWidth) centerX = - wm.conf.borderWidth;
  if(centerY < - wm.conf.borderWidth) centerY = - wm.conf.borderWidth;

  c->inLayout = false;
  c->fullscreen = false;

  if(c->frame != 0){
    XMoveResizeWindow(wm.display, c->frame, centerX, centerY, (int)ww, (int)wh);
  }
  XResizeWindow(wm.display, c->win, (int)ww, (int)wh);
}

void handleMapRequest(XEvent *ev){
  XMapRequestEvent *e = &ev->xmaprequest;

  XWindowAttributes attr;
  if(!XGetWindowAttributes(wm.display, e->window, &attr)) return;

  xwmWindowFrame(e->window);

  int32_t idx = getClientIndex(e->window);
  if(idx == -1){
    XMapWindow(wm.display, e->window);
    return;
  }

  Client *c = &wm.client_windows[idx];

  applyRules(c);

  int desktopIndex = c->desktopIndex - 1;
  XChangeProperty(wm.display, c->win,
    XInternAtom(wm.display, "_NET_WM_DESKTOP", False),
    XA_CARDINAL, 32, PropModeReplace,
    (unsigned char *)&desktopIndex, 1);

  Window transientFor = None;
  if(XGetTransientForHint(wm.display, c->win, &transientFor) && transientFor != None){
    c->inLayout = false;
    if(c->frame == None) xwmWindowFrame(c->win);
  }

  if(!c->inLayout){
    manageFloatingWindow(c);
    if(c->frame != None) XMapWindow(wm.display, c->frame);
    XMapWindow(wm.display, c->win);
  }
  else{
    if(c->frame != None) XMapWindow(wm.display, c->frame);
    XMapWindow(wm.display, c->win);
    retileLayout();
  }

  XSync(wm.display, false);

  if(c->fullscreen){
    setFullscreen(c->win);
  }

  if(c->frame != None) XRaiseWindow(wm.display, c->frame);
  setFocusToWindow(c->win);

  updateClientList();
}

void handleUnmapNotify(XEvent *ev){
  XUnmapEvent *e = &ev->xunmap;

  if(clientFrameExists(e->window)){
    return;
  }

  Atom strut_atom = XInternAtom(wm.display, "_NET_WM_STRUT_PARTIAL", false);
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;

  if(XGetWindowProperty(wm.display, e->window, strut_atom, 0, 12,
                        false, XA_CARDINAL, &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop) == Success){
    if(prop){
      XFree(prop);
      recalculateFixedPartialStrut();
    }
  }

  if(clientWindowExists(e->window)){
    int32_t idx = getClientIndex(e->window);
    if(idx == -1) return;

    int32_t desktopIndex = wm.client_windows[idx].desktopIndex;
    xwmWindowUnframe(e->window);
    focusNextWindow();

    if(!desktopHasWindows(desktopIndex)){
      XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
      updateActiveWindow(None);
    }
    XSync(wm.display, false);
  }
}

void handleDestroyNotify(XEvent *ev){
  XDestroyWindowEvent *e = &ev->xdestroywindow;
  if(clientWindowExists(e->window)){
    int32_t id = getClientIndex(e->window);
    if(id != -1){
      int32_t desktopIndex = wm.client_windows[id].desktopIndex;
      xwmWindowUnframe(e->window);
      focusNextWindow();
      if(!desktopHasWindows(desktopIndex)){
        XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
        updateActiveWindow(None);
      }
    }
  }

  //maybe a bad way of doing things, TODO : maybe perforamnce nom nom here
  if(wm.conf.topBorder > 0 || wm.conf.bottomBorder > 0){
    recalculateFixedPartialStrut();
  }
  XSync(wm.display, false);
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


void xwmWindowFrame(Window win){

  if(clientWindowExists(win) || clientFrameExists(win)) return;

  XWindowAttributes attribs;
  if(!XGetWindowAttributes(wm.display, win, &attribs)) return;

  bool override_redirect = attribs.override_redirect;

  Atom aTypeNet =          XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE", False);
  Atom aTypeDialog =       XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  Atom aTypeUtility =      XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
  Atom aTypeSplash =       XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_SPLASH", False);

  Atom aTypeMenu =         XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_MENU", False);
  Atom aTypePopupMenu =    XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
  Atom aTypeDropDown =     XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
  Atom aTypeTooltip =      XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
  Atom aTypeCombo =        XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_COMBO", False);
  Atom aTypeDock =         XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_DOCK", False);
  Atom aTypeNotification = XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);

  unsigned char *prop = NULL;
  Atom actualType;
  int actualFormat;
  unsigned long nitems, bytes_after;
  bool isDialogLike = false;
  bool isMenuLike = false;
  bool isDockLike = false;

  if(XGetWindowProperty(wm.display, win,
                        aTypeNet, 0,
                        (~0L), False,
                        AnyPropertyType, &actualType,
                        &actualFormat, &nitems,
                        &bytes_after, &prop) == Success && prop){

    Atom *atoms = (Atom*)prop;
    for(unsigned long i = 0; i < nitems; ++i){
      if(atoms[i] == aTypeDialog   || atoms[i] == aTypeUtility || atoms[i] == aTypeSplash){
        isDialogLike = true;
      }
      if(atoms[i] == aTypeMenu     || atoms[i] == aTypePopupMenu ||
         atoms[i] == aTypeDropDown || atoms[i] == aTypeTooltip ||
         atoms[i] == aTypeCombo    || atoms[i] == aTypeNotification){
        isMenuLike = true;
      }
      if(atoms[i] == aTypeDock){
        isDockLike = true;
      }
    }
    
    XFree(prop);
    prop = NULL;

  }

  Window transientFor = None;
  bool has_transient = (XGetTransientForHint(wm.display, win, &transientFor) && transientFor != None);

  bool shouldManage = true;
  if(isDockLike){
    shouldManage = false;
    FGetFixedPartialStrut(win);
  }
  if(isMenuLike && !has_transient){
    shouldManage = false;
    FGetFixedPartialStrut(win);
  }
  if(!isMenuLike && !isDialogLike && override_redirect){
    shouldManage = false;
  }

  if(!shouldManage){
    return;
  }

  if(wm.clients_count >= CLIENT_WINDOW_CAP) return;

  int wwidth = attribs.width > 0 ? attribs.width : 400;
  int wheight = attribs.height > 0 ? attribs.height : 300;

  int window_x = attribs.x;
  int window_y = attribs.y;
  if(wm.currentLayout == WINDOW_LAYOUT_FLOATING){
    window_x = (wm.screenWidth - wwidth) / 2;
    window_y = (wm.screenHeight - wheight) / 2;
  }

  Window winFrame = XCreateSimpleWindow(
    wm.display,
    wm.root,
    window_x,
    window_y,
    wwidth,
    wheight,
    wm.conf.borderWidth,
    wm.conf.borderUnfocused,
    BG_COLOR
  );

  XSelectInput(wm.display, win,
               StructureNotifyMask | PropertyChangeMask | EnterWindowMask | 
               FocusChangeMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask);

  XReparentWindow(wm.display, win, winFrame, 0, 0);

  XSelectInput(wm.display, winFrame,
               SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask |
               ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask);

  XAddToSaveSet(wm.display, win);

  XResizeWindow(wm.display, win, wwidth, wheight);

  memset(&wm.client_windows[wm.clients_count], 0, sizeof(Client));
  Client *c = &wm.client_windows[wm.clients_count];
  c->frame = winFrame;
  c->win = win;
  c->fullscreen = false;
  c->desktopIndex = wm.currentDesktop;
  c->forceManage = (has_transient || isDialogLike);
  c->inLayout = isLayoutTiling(wm.currentLayout) && !c->forceManage;


  wm.clients_count++;

  XGrabButton(wm.display, Button1, MOD, win, False,
              ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.display, Button3, MOD, win, False,
              ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);

  XSync(wm.display, False);
}

void xwmWindowUnframe(Window w){
  int32_t idx = getClientIndex(w);

  if(idx != -1){
    if(wm.client_windows[idx].frame == w){
      w = wm.client_windows[idx].win;
    }
  }

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
  wm.clients_count--;

  retileLayout();
  updateClientList();
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

  setFocusToWindow(w);
  Window frame = getFrameWindow(w);
  XSetWindowBorderWidth(wm.display, frame, 0);
  int screen = DefaultScreen(wm.display);
  int screenWidth = DisplayWidth(wm.display, screen);
  int screenHeight = DisplayHeight(wm.display, screen);
  XMoveResizeWindow(wm.display,
                    frame,
                    0,
                    0,
                    screenWidth,
                    screenHeight);
  XResizeWindow(wm.display, w, screenWidth, screenHeight);
  XSync(wm.display, false);
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
  
  int centerX = ((screenWidth - windowWidth) / 2) - wm.conf.borderWidth;
  int centerY = ((screenHeight - windowHeight) / 2) - wm.conf.borderWidth;
  
  XMoveResizeWindow(wm.display, frame, centerX, centerY, windowWidth, windowHeight);
  XResizeWindow(wm.display, w, windowWidth, windowHeight);

  retileLayout();
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
  updateDesktopProperties();
}

void moveWindowToDesktop(Window w, int32_t desktopIndex){
  if(desktopIndex < 0 || desktopIndex >= DESKTOP_COUNT) return;

  int32_t clientIndex = getClientIndex(w);
  if(clientIndex == -1) return;
  Client *client = &wm.client_windows[clientIndex];
  int32_t oldDesktop = client->desktopIndex;
  client->desktopIndex = desktopIndex;

  int desktopIndexProperty = client->desktopIndex - 1;
  XChangeProperty(wm.display, client->win,
    XInternAtom(wm.display, "_NET_WM_DESKTOP", False),
    XA_CARDINAL, 32, PropModeReplace,
    (unsigned char *)&desktopIndexProperty, 1);

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
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER || wm.currentLayout == WINDOW_LAYOUT_FLOATING){
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
}



void moveClientDownLayout(Client *client){
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER || wm.currentLayout == WINDOW_LAYOUT_FLOATING){
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
}

void moveSlavesStackUp(Client *client){
  (void)client;
  int32_t indices[CLIENT_WINDOW_CAP];
  int32_t count = 0;
  int32_t masterIndex = -1;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
      masterIndex = (int32_t)i;
      break;
    }
  }
  if(masterIndex == -1) return;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if((int32_t)i == masterIndex) continue;
      if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
        indices[count++] = (int32_t)i;
      }
  }
  if(count <= 1) return;

  Client tmp = wm.client_windows[indices[0]];
  for(int32_t k = 0; k < count - 1; k++){
    wm.client_windows[indices[k]] = wm.client_windows[indices[k + 1]];
  }
  wm.client_windows[indices[count - 1]] = tmp;
  retileLayout();
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_CASCADE){
    fixFocusForCascade();
  }
}

void moveSlavesStackDown(Client *client){
  (void)client;
  int32_t indices[CLIENT_WINDOW_CAP];
  int32_t count = 0;
  int32_t masterIndex = -1;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
      masterIndex = (int32_t)i;
      break;
    }
  }
  if(masterIndex == -1) return;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if((int32_t)i == masterIndex) continue;
      if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
        indices[count++] = (int32_t)i;
      }
  }
  if(count <= 1) return;

  Client tmp = wm.client_windows[indices[count - 1]];
  for(int32_t k = count - 1; k > 0; k--){
    wm.client_windows[indices[k]] = wm.client_windows[indices[k - 1]];
  }
  wm.client_windows[indices[0]] = tmp;
  retileLayout();
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_CASCADE){
    fixFocusForCascade();
  }
}

void fixFocusForCascade(){
  Window restack[CLIENT_WINDOW_CAP];
  int32_t indices[CLIENT_WINDOW_CAP];
  int count = 0;
  int32_t masterIndex = -1;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
      masterIndex = (int32_t)i;
      break;
    }
  }

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
      Window frame = wm.client_windows[i].frame ? wm.client_windows[i].frame : wm.client_windows[i].win;
      restack[count] = frame;
      indices[count] = (int32_t)i;
      count++;
    }
  }

  if(count == 0) return;
  
  for(int a = 0; a < count / 2; a++){// confusing loop lol
    int b = count - 1 - a;
    Window tmpw = restack[a];
    restack[a] = restack[b];
    restack[b] = tmpw;

    int32_t tmpi = indices[a];
    indices[a] = indices[b];
    indices[b] = tmpi;
  }
 
  XRestackWindows(wm.display, restack, count); 

  int32_t focusClientIdx = -1;
  for(int i = 0; i < count; i++){
    if(indices[i] != masterIndex){
      focusClientIdx = indices[i];// window below?(or above depending on how you look at it) other slaves
      break;
    }
  }

  if(focusClientIdx == -1){
    if(masterIndex != -1){
      focusClientIdx = masterIndex;
    } 
    else{
      focusClientIdx = indices[count - 1];
    }
  }

  if(focusClientIdx >= 0){
    Window focusWin = wm.client_windows[focusClientIdx].win;
    if(wm.focused_Window != focusWin){
      updateWindowBorders(focusWin);
      XSetInputFocus(wm.display, focusWin, RevertToParent, CurrentTime);
    }
  }

  XFlush(wm.display);
}


void swapMasterWithSlave(Client *client){
  int32_t clientIndex = getClientIndex(client->win);
  if(clientIndex == -1 || wm.clients_count <= 1) return;

  int32_t masterIndex = -1;
  int32_t lastClientIndex = -1;
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].desktopIndex == wm.currentDesktop && wm.client_windows[i].inLayout){
      if(masterIndex == -1) masterIndex = i;
      lastClientIndex = i;
    }
  }
  if(masterIndex == -1) return;

  if(wm.currentLayout == WINDOW_LAYOUT_TILED_CASCADE && clientIndex == masterIndex){
    Client tmp = wm.client_windows[masterIndex];
    wm.client_windows[masterIndex] = wm.client_windows[lastClientIndex];
    wm.client_windows[lastClientIndex] = tmp;
    fixFocusForCascade();
    retileLayout();
    return;
  }

  Client tmp = wm.client_windows[masterIndex];
  wm.client_windows[masterIndex] = wm.client_windows[clientIndex];
  wm.client_windows[clientIndex] = tmp;
  fixFocusForCascade();
  retileLayout();
}



Client *getFocusedSlaveClient(){
  if(wm.focused_Window == None) return NULL;
  if(!clientWindowExists(wm.focused_Window)) return NULL;

  int32_t focusedIndex = getClientIndex(wm.focused_Window);
  if(focusedIndex == -1) return NULL;

  Client *focusedClient = &wm.client_windows[focusedIndex];
  if(!focusedClient->inLayout || focusedClient->desktopIndex != wm.currentDesktop){
    return NULL;
  }

  Client *clients[CLIENT_WINDOW_CAP];
  uint32_t client_count = 0;
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].inLayout && wm.client_windows[i].desktopIndex == wm.currentDesktop){
      clients[client_count++] = &wm.client_windows[i];
    }
  }

  if((client_count <= 1) || (clients[0] == focusedClient)){
    return NULL;
  }

  return focusedClient;
}

void retileLayout(){
  polybarLayoutIPC(wm.currentLayout);

  Client *clients[CLIENT_WINDOW_CAP];
  uint32_t client_count = 0;
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].inLayout && wm.client_windows[i].desktopIndex == wm.currentDesktop){
      clients[client_count++] = &wm.client_windows[i];
    }
  }
  if(client_count == 0) return;

  int availableHeight = wm.screenHeight - wm.conf.topBorder - wm.conf.bottomBorder;
  int startY = wm.conf.topBorder;

  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER){
    Client *master = clients[0];

    if(client_count == 1){
      XMoveWindow(wm.display, master->frame, wm.conf.windowGap - wm.conf.borderWidth, startY + wm.conf.windowGap - wm.conf.borderWidth);
      sendConfigureNotify(master);
      int frameWidth = wm.screenWidth - 2 * wm.conf.windowGap;
      int frameHeight = availableHeight - 2 * wm.conf.windowGap;
      XResizeWindow(wm.display, master->frame, frameWidth, frameHeight);
      XResizeWindow(wm.display, master->win, frameWidth, frameHeight);
      XSetWindowBorderWidth(wm.display, master->frame, wm.conf.borderWidth);
      if(master->fullscreen){
        master->fullscreen = false;
      }
      return;
    } 
    int32_t masterX, stackX;
    if(wm.conf.masterLeft){
      masterX = wm.conf.windowGap;
      stackX = (wm.screenWidth / 2 + (wm.conf.windowGap / 2)) + wm.conf.masterWindowGap;
    }
    else{
      masterX = (wm.screenWidth / 2 + (wm.conf.windowGap / 2)) - wm.conf.masterWindowGap;
      stackX = wm.conf.windowGap;
    }
    XMoveWindow(wm.display, master->frame, masterX - wm.conf.borderWidth, startY + wm.conf.windowGap - wm.conf.borderWidth);
    sendConfigureNotify(master);
    int masterFrameWidth = wm.screenWidth/2 - wm.conf.windowGap - (wm.conf.windowGap/2) + wm.conf.masterWindowGap;
    int masterFrameHeight = availableHeight - 2 * wm.conf.windowGap;
    XResizeWindow(wm.display, master->frame, masterFrameWidth, masterFrameHeight);
    XResizeWindow(wm.display, master->win, masterFrameWidth, masterFrameHeight);
    XSetWindowBorderWidth(wm.display, master->frame, wm.conf.borderWidth);
    master->fullscreen = false;

    int32_t stackFrameWidth = wm.screenWidth/2 - wm.conf.windowGap - (wm.conf.windowGap / 2) - wm.conf.masterWindowGap;
    int32_t stackFrameHeight = (availableHeight - ((client_count) * wm.conf.windowGap)) / (client_count - 1);

    for(uint32_t i = 1; i < client_count; i++){
      Client *client = clients[i];
      int32_t stackY = wm.conf.windowGap + (i - 1) * (stackFrameHeight + wm.conf.windowGap);

      XMoveWindow(wm.display, client->frame, stackX - wm.conf.borderWidth, startY + stackY - wm.conf.borderWidth);
      sendConfigureNotify(client);
      XResizeWindow(wm.display, client->frame, stackFrameWidth, stackFrameHeight);
      XResizeWindow(wm.display, client->win, stackFrameWidth, stackFrameHeight);
      XSetWindowBorderWidth(wm.display, client->frame, wm.conf.borderWidth);

      if(client->fullscreen){
        client->fullscreen = false;
      }
    }
    
    if(wm.focused_Window != None && clientWindowExists(wm.focused_Window)){
      updateWindowBorders(wm.focused_Window);
    }
    XSync(wm.display, false);
  }
  else if(wm.currentLayout == WINDOW_LAYOUT_HORIZONTAL_MASTER){
    Client *master = clients[0];

    if(client_count == 1){
      XMoveWindow(wm.display, master->frame, wm.conf.windowGap - wm.conf.borderWidth, startY + wm.conf.windowGap - wm.conf.borderWidth);
      sendConfigureNotify(master);
      int frameWidth = wm.screenWidth - 2 * wm.conf.windowGap;
      int frameHeight = availableHeight - 2 * wm.conf.windowGap;
      XResizeWindow(wm.display, master->frame, frameWidth, frameHeight);
      XResizeWindow(wm.display, master->win, frameWidth, frameHeight);
      XSetWindowBorderWidth(wm.display, master->frame, wm.conf.borderWidth);
      if(master->fullscreen){
        master->fullscreen = false;
      }
      return;
    }

    int baseStackHeight = availableHeight / 2;
    int stackHeight = baseStackHeight + wm.conf.masterWindowGap;
    int masterHeight = availableHeight - stackHeight - (2 * wm.conf.windowGap);

    int masterX = wm.conf.windowGap;
    int masterY, stackY;
    
    if(wm.conf.masterLeft){
      masterY = startY + stackHeight + wm.conf.windowGap;
      stackY = startY + wm.conf.windowGap;
    }
    else{
      masterY = startY + wm.conf.windowGap;
      stackY = startY + masterHeight + (2 * wm.conf.windowGap);
    }
    
    int masterW = wm.screenWidth - (2 * wm.conf.windowGap);
    int masterH = masterHeight;

    XMoveWindow(wm.display, master->frame, masterX - wm.conf.borderWidth, masterY - wm.conf.borderWidth);
    XResizeWindow(wm.display, master->frame, masterW, masterH);
    XResizeWindow(wm.display, master->win, masterW, masterH);
    XSetWindowBorderWidth(wm.display, master->frame, wm.conf.borderWidth);
    sendConfigureNotify(master);
    master->fullscreen = false;

    int stackX = wm.conf.windowGap;
    int stackW = (wm.screenWidth - (2 * wm.conf.windowGap)) / (client_count - 1);
    int stackH = stackHeight - (2 * wm.conf.windowGap);

    for(uint32_t i = 1; i < client_count; i++){
      Client *client = clients[i];
      int startx = stackX + (stackW * (i - 1));
      int width = (i == client_count - 1) ? (uint32_t)stackW : stackW - wm.conf.windowGap;
      XMoveWindow(wm.display, client->frame, startx - wm.conf.borderWidth, stackY - wm.conf.borderWidth);
      XResizeWindow(wm.display, client->frame, width, stackH);
      XResizeWindow(wm.display, client->win, width, stackH);
      XSetWindowBorderWidth(wm.display, client->frame, wm.conf.borderWidth);
      sendConfigureNotify(client);
      client->fullscreen = false;
    }

    if(wm.focused_Window != None && clientWindowExists(wm.focused_Window)){
        updateWindowBorders(wm.focused_Window);
    }
    XSync(wm.display, false);
  }
  else if(wm.currentLayout == WINDOW_LAYOUT_TILED_CASCADE){
    Client *master = clients[0];

    if(client_count == 1){
      XMoveWindow(wm.display, master->frame, wm.conf.windowGap - wm.conf.borderWidth, startY + wm.conf.windowGap - wm.conf.borderWidth);
      sendConfigureNotify(master);
      int frameWidth = wm.screenWidth - 2 * wm.conf.windowGap;
      int frameHeight = availableHeight - 2 * wm.conf.windowGap;
      XResizeWindow(wm.display, master->frame, frameWidth, frameHeight);
      XResizeWindow(wm.display, master->win, frameWidth, frameHeight);
      XSetWindowBorderWidth(wm.display, master->frame, wm.conf.borderWidth);
      master->fullscreen = false;
      return;
    }

    int32_t masterX, stackX;
    int masterFrameWidth = wm.screenWidth/2 - wm.conf.windowGap - (wm.conf.windowGap/2) + wm.conf.masterWindowGap;
    
    if(wm.conf.masterLeft){
      masterX = wm.conf.windowGap;
      stackX = (wm.screenWidth / 2 + (wm.conf.windowGap / 3)) + wm.conf.masterWindowGap;
    }
    else{
      masterX = (wm.screenWidth / 2 + (wm.conf.windowGap / 3)) - wm.conf.masterWindowGap;
      stackX = wm.conf.windowGap;
    }

    XMoveWindow(wm.display, master->frame, masterX - wm.conf.borderWidth, startY + wm.conf.windowGap - wm.conf.borderWidth);
    sendConfigureNotify(master);
    int masterFrameHeight = availableHeight - 2 * wm.conf.windowGap;
    XResizeWindow(wm.display, master->frame, masterFrameWidth, masterFrameHeight);
    XResizeWindow(wm.display, master->win, masterFrameWidth, masterFrameHeight);
    XSetWindowBorderWidth(wm.display, master->frame, wm.conf.borderWidth);
    master->fullscreen = false;

    int offsetX = wm.conf.windowGap / 2;
    int offsetY = wm.conf.windowGap / 2;

    if(offsetX < 20) offsetX = 20;
    if(offsetY < 20) offsetY = 20;

    int baseWidth  = (wm.screenWidth - masterFrameWidth - (2 * wm.conf.windowGap) - (wm.conf.windowGap / 3) - (client_count - 2) * offsetX) - wm.conf.windowGap;
    int baseHeight = availableHeight - 2 * wm.conf.windowGap - (client_count - 2) * offsetY;

    if(baseWidth < 50) baseWidth = 50;
    if(baseHeight < 30) baseHeight = 30;

    for(uint32_t i = 1; i < client_count; i++){
      Client *client = clients[i];

      int x = stackX + (i - 1) * offsetX;
      int y = startY + wm.conf.windowGap + (i - 1) * offsetY;

      XMoveWindow(wm.display, client->frame, x - wm.conf.borderWidth, y - wm.conf.borderWidth);
      sendConfigureNotify(client);
      XResizeWindow(wm.display, client->frame, baseWidth, baseHeight);
      XResizeWindow(wm.display, client->win, baseWidth, baseHeight);
      XSetWindowBorderWidth(wm.display, client->frame, wm.conf.borderWidth);

      client->fullscreen = false;
    }

    if(wm.focused_Window != None && clientWindowExists(wm.focused_Window)){
      updateWindowBorders(wm.focused_Window);
    }
    XSync(wm.display, false);
  }
}

void applyRules(Client *c){
  if(!c) return;

  c->inLayout = isLayoutTiling(wm.currentLayout);
  c->forceManage = false;

  XClassHint ch = {0};
  if(XGetClassHint(wm.display, c->win, &ch)){
    if(ch.res_name){
      if(strcmp(ch.res_name, "dialog") == 0 || strcmp(ch.res_name, "confirm") == 0){
        c->inLayout = false;
        c->forceManage = true;
      }
    }
    if(ch.res_class){
      if(strcmp(ch.res_class, "Xmessage") == 0 || strcmp(ch.res_class, "Pavucontol") == 0){
        c->inLayout = false;
        c->forceManage = true;
      }
    }
    if(ch.res_name)  XFree(ch.res_name);
    if(ch.res_class) XFree(ch.res_class);
  }

  Window transientFor = None;
  if(XGetTransientForHint(wm.display, c->win, &transientFor) && transientFor != None){
    c->inLayout = false;
    c->forceManage = true;
  }

  Atom actualType;
  int actualFormat;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;

  Atom aTypeNet =     XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE", false);
  Atom aTypeDialog =  XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_DIALOG", false);
  Atom aTypeUtility = XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_UTILITY", false);
  Atom aTypeSplash =  XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_SPLASH", false);

  if(XGetWindowProperty(wm.display, c->win, aTypeNet, 0, (~0L), false, AnyPropertyType, &actualType, &actualFormat, &nitems, &bytes_after, &prop) == Success && prop){
    Atom *atoms = (Atom*)prop;
    for(unsigned long i = 0; i < nitems; ++i){
      if(atoms[i] == aTypeDialog || atoms[i] == aTypeUtility || atoms[i] == aTypeSplash){
        c->inLayout = false;
        c->forceManage = true;
        break;
      }
    }
    XFree(prop);
    prop = NULL;
  }
  
  Atom aStateNet = XInternAtom(wm.display, "_NET_WM_STATE", false);
  Atom aStateFs =  XInternAtom(wm.display, "_NET_WM_STATE_FULLSCREEN", false);

  if(XGetWindowProperty(wm.display, c->win, aStateNet, 0, (~0L), false, AnyPropertyType, &actualType, &actualFormat, &nitems, &bytes_after, &prop) == Success && prop){
    Atom *states = (Atom*)prop;
    for(unsigned long i = 0; i < nitems; ++i){
      if(states[i] == aStateFs){
        c->fullscreen = true;
        break;
      }
    }
    XFree(prop);
    prop = NULL;
  }
  
  if(wm.currentLayout == WINDOW_LAYOUT_FLOATING) c->inLayout = false;
}

bool isLayoutTiling(WindowLayout layout){
  switch(layout){
    case WINDOW_LAYOUT_TILED_MASTER:
      return true;
    case WINDOW_LAYOUT_TILED_CASCADE:
      return true;
    case WINDOW_LAYOUT_HORIZONTAL_MASTER:
      return true;
    case WINDOW_LAYOUT_FLOATING:
      return false;
    default:
      return true;
  }
}

int main(void){
  pthread_t inotify_thread;
  wm = xwm_init();
  if(XRESOURCES_AUTO_RELOAD){
    pthread_create(&inotify_thread, NULL, inotifyXresources, NULL);
  }
  xwm_run();
  return 0;
}
