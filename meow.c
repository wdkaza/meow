#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/cursorfont.h>
#include "config.h"
// thanks @cococry for some of the code
// @cococry also insipered me to try to code this 
// like actually massive props to cococry's code 
// it helpmed me so much to understand how to even start making a wm

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

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
} Client;

typedef struct {
  Display* display;
  Window root;

  bool running;

  Client client_windows[CLIENT_WINDOW_CAP];
  uint32_t clients_count;

  Vec2 cursor_start_pos;
  Vec2 cursor_start_frame_pos;
  Vec2 cursor_start_frame_size;

  WindowLayout currentLayout;
  int windowGap;

  Atom ATOM_WM_DELETE_WINDOW; // remove later(useless mostly)
  Atom ATOM_WM_PROTOCOLS; // remove later(useless mostly)
} XWM;

void handleConfigureRequst(XEvent *ev);
void handleUnmapNotify(XEvent *ev);
void handleDestroyNotify(XEvent *ev);
void handleConfigureNotify(XEvent *ev);
void handleMapRequest(XEvent *ev);
void xwmWindowFrame(Window win, bool createdBeforeWindowManager);
void xwmWindowUnframe(Window w);
void handleButtonPress(XEvent *ev);
void handleKeyRelease(XEvent *ev);
void handleMotionNotify(XEvent *ev);
void handleKeyPress(XEvent *ev);
void retileLayout();
void sendClientMessage(Window w, Atom a);
Window getFrameWindow(Window w);
int32_t getClientIndex(Window w);
bool clientFrameExists(Window w);
bool clientWindowExists(Window w);
void unsetFullscreen(Window w);
void setFullscreen(Window w);
Atom* findAtomPtrRange(Atom* ptr1, Atom* ptr2, Atom val);

static XWM wm;

int xwm_error_handler(Display *dpy, XErrorEvent *e){
  char buf[1024];
  XGetErrorText(dpy, e->error_code, buf, sizeof(buf));
  fprintf(stderr, "X Error: %s (Request: %d)\n", buf, e->error_code);
  return 0;
}

XWM xwm_init(){
  XWM wm;
  XSetErrorHandler(xwm_error_handler);

  wm.display = XOpenDisplay(NULL);
  if(!wm.display){
    printf("failed to open x display\n");
    exit(1);
  }

  wm.root = DefaultRootWindow(wm.display);
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
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, TERMINAL_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, FULLSCREEN_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync); 
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, CYCLE_WINDOWS_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, GAP_INCREASE_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, GAP_DECREAS_KEY), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_TILED_MASTER), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_FLOATING), MASTER_KEY, wm.root, false, GrabModeAsync, GrabModeAsync);

  
  wm.clients_count = 0;
  wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.windowGap = START_WINDOW_GAP;
  wm.ATOM_WM_PROTOCOLS = XInternAtom(wm.display, "WM_PROTOCOLS", false);
  wm.ATOM_WM_DELETE_WINDOW = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);




  while(wm.running){
    XEvent ev;
    XNextEvent(wm.display, &ev);

    switch(ev.type){
      default:
        puts("unexpected event");
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
  }
}

void handleButtonPress(XEvent *ev){
  XButtonEvent *e = &ev->xbutton;

  if(!clientWindowExists(e->window)) return;

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

  if((e->state & MASTER_KEY) && (e->state & Button1Mask)){
    XMoveWindow(wm.display, frame, 
                (int)(wm.cursor_start_frame_pos.x) + dx, 
                (int)(wm.cursor_start_frame_pos.y) + dy
    );
  }
  else if((e->state & MASTER_KEY) && (e->state & Button3Mask)){
    printf("dx: %d, dy: %d\n", dx, dy);
    printf("dx1: %f, dy1: %f\n", -wm.cursor_start_frame_size.x, -wm.cursor_start_frame_size.y);
    Vec2 rdy = (Vec2){.x = wm.cursor_start_frame_size.x + MAX(dx, -wm.cursor_start_frame_size.x),
                      .y = wm.cursor_start_frame_size.y + MAX(dy, -wm.cursor_start_frame_size.y)};
    XResizeWindow(wm.display, frame, (int)rdy.x, (int)rdy.y);
    XResizeWindow(wm.display, e->window, (int)rdy.x, (int)rdy.y);
  }

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
    XConfigureWindow(wm.display, frame_win, e->value_mask, &changes);
    XConfigureWindow(wm.display, e->window, e->value_mask, &changes);
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

  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, KILL_KEY)){
    if(clientWindowExists(focusedWindow)){
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
        XRaiseWindow(wm.display, wm.client_windows[nextIndex].frame);
        XSetInputFocus(wm.display, wm.client_windows[nextIndex].win, RevertToParent, CurrentTime);
      }
      else{// no more windows, focus root
        XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
      }
    }
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, TERMINAL_KEY)){
    system(TERMINAL_CMD);
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, GAP_INCREASE_KEY)){
    wm.windowGap += 2;
    retileLayout();
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, GAP_DECREAS_KEY)){
    wm.windowGap -= 2;
    retileLayout();
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_TILED_MASTER)){
    wm.currentLayout = WINDOW_LAYOUT_TILED_MASTER;
    retileLayout();
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, SET_WINDOW_LAYOUT_FLOATING)){
    wm.currentLayout = WINDOW_LAYOUT_FLOATING;
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, FULLSCREEN_KEY)){
    uint32_t clientIndex = getClientIndex(focusedWindow);
    if(!wm.client_windows[clientIndex].fullscreen){
      setFullscreen(focusedWindow);
    }
    else{
      unsetFullscreen(focusedWindow);
    }
  }
  if(e->state & MASTER_KEY && e->keycode == XKeysymToKeycode(wm.display, CYCLE_WINDOWS_KEY)){
    Client client = {0};
    for(uint32_t i = 0; i < wm.clients_count; i++){
      if(wm.client_windows[i].win == focusedWindow || wm.client_windows[i].frame == focusedWindow){
        if(i+1 >= wm.clients_count){
          client = wm.client_windows[0];
        }
        else{
             client = wm.client_windows[i+1];
        }
        break;
      }
    }
    XRaiseWindow(wm.display, client.frame);
    XSetInputFocus(wm.display, client.win, RevertToParent, CurrentTime);

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
  XSetInputFocus(wm.display, e->window, RevertToParent, CurrentTime);
}

void handleUnmapNotify(XEvent *ev){
  XUnmapEvent *e = &ev->xunmap;
  if(!clientWindowExists(e->window) || e->event == wm.root){
    printf("ignoring unmap, not a client window\n");
    return;
  }

  Window focusedWindow;
  int revert_to;
  XGetInputFocus(wm.display, &focusedWindow, &revert_to);
  bool was_focused = (focusedWindow == e->window);

  uint32_t closingIndex = getClientIndex(e->window);
  if(was_focused && wm.clients_count > 0){
      uint32_t next_index = closingIndex;
      if(next_index >= wm.clients_count){
        next_index = wm.clients_count - 1;
      }
      XRaiseWindow(wm.display, wm.client_windows[next_index].frame);
      XSetInputFocus(wm.display, wm.client_windows[next_index].win, RevertToParent, CurrentTime);
  }
  else if(wm.clients_count == 0){
      XSetInputFocus(wm.display, wm.root, RevertToParent, CurrentTime);
    }
  xwmWindowUnframe(e->window);
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
  Window frameWin = getFrameWindow(w);
  XUnmapWindow(wm.display, frameWin);
  XReparentWindow(wm.display, w, wm.root, 0, 0);
  XRemoveFromSaveSet(wm.display, w);
  XDestroyWindow(wm.display, frameWin);
  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(wm.client_windows[i].win == w){
      for(uint32_t j = i; j < wm.clients_count - 1; j++){
        wm.client_windows[j]= wm.client_windows[j+1];
      }
      wm.clients_count--;
      retileLayout();
      break;
    }
  }
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
  XSetWindowBorderWidth(wm.display, frame, 0);
  int screen = DefaultScreen(wm.display);
  int screenWidth = DisplayWidth(wm.display, screen);
  int screenHeight = DisplayHeight(wm.display, screen);
  // BUG HERE (full screen right now only works with gaps TODO: make actual fullscreen toggable) 
  XMoveResizeWindow(wm.display, frame, wm.windowGap, wm.windowGap, screenWidth - wm.windowGap * 2, screenHeight - wm.windowGap * 2);
  XResizeWindow(wm.display, w, screenWidth - wm.windowGap * 2, screenHeight - wm.windowGap * 2);
}

void unsetFullscreen(Window w){
  uint32_t clientIndex = getClientIndex(w);
  if(!wm.client_windows[clientIndex].fullscreen) return;
  wm.client_windows[clientIndex].fullscreen = false;
  Window frame = getFrameWindow(w);
  XSetWindowBorderWidth(wm.display, frame, BORDER_WIDTH);
  XMoveResizeWindow(wm.display, frame, 0, 0, wm.client_windows[clientIndex].fullscreenRevertSize.x, wm.client_windows[clientIndex].fullscreenRevertSize.y); // BUG HERE BUG HERE BUG HERE BUG HERE BUG HERE BUG HERE BUG HERE BUG HERE(we always move window back to 0,0 (x,y) and it should go back ot its position prob, but whatever)
  XResizeWindow(wm.display, w, wm.client_windows[clientIndex].fullscreenRevertSize.x, wm.client_windows[clientIndex].fullscreenRevertSize.y);
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

void retileLayout(){
  int32_t masterIndex = -1;
  uint32_t clientsVisible = wm.clients_count;
  bool foundMaster = false;

  for(uint32_t i = 0; i < wm.clients_count; i++){
    if(!foundMaster){
      masterIndex = i;
      foundMaster = true;
    }
  }
  
  if(clientsVisible <= 0 || !foundMaster) return;

  Client *master = &wm.client_windows[masterIndex];
  if(wm.currentLayout == WINDOW_LAYOUT_TILED_MASTER){
    if(clientsVisible == 1){
      XMoveWindow(wm.display, master->frame, wm.windowGap, wm.windowGap);
      XResizeWindow(wm.display, master->frame, 
                    MONITOR_WIDTH - 2 * wm.windowGap, 
                    MONITOR_HEIGHT - 2 * wm.windowGap);
      XResizeWindow(wm.display, master->win, 
                    MONITOR_WIDTH - 2 * wm.windowGap, 
                    MONITOR_HEIGHT - 2 * wm.windowGap);
      XSetWindowBorderWidth(wm.display, master->frame, BORDER_WIDTH);
      master->fullscreen = false;
      return;
    } 
  
    XMoveWindow(wm.display, master->frame, wm.windowGap, wm.windowGap);
    XResizeWindow(wm.display, master->frame, MONITOR_WIDTH/2 - 2 * wm.windowGap, MONITOR_HEIGHT - 2 * wm.windowGap);// change with actual values later
    XResizeWindow(wm.display, master->win, MONITOR_WIDTH/2, MONITOR_HEIGHT);
    XSetWindowBorderWidth(wm.display, master->frame, BORDER_WIDTH);
    master->fullscreen = false;

    for(uint32_t i = 1; i < clientsVisible; i++){
      uint32_t clientIndex = masterIndex + i;
      int32_t stack_width  = MONITOR_WIDTH / 2 - 2 * wm.windowGap;
      int32_t stack_height = (MONITOR_HEIGHT - (clientsVisible * wm.windowGap)) / (clientsVisible - 1);
      int32_t stack_x      = MONITOR_WIDTH / 2 + wm.windowGap;
      int32_t stack_y      = wm.windowGap + (i - 1) * (stack_height + wm.windowGap);
      XResizeWindow(wm.display, wm.client_windows[clientIndex].frame,
                    stack_width, stack_height);
      XResizeWindow(wm.display, wm.client_windows[clientIndex].win,
                    stack_width, stack_height);

      XMoveWindow(wm.display, wm.client_windows[clientIndex].frame, stack_x, stack_y);
      XSetWindowBorderWidth(wm.display, wm.client_windows[clientIndex].frame, BORDER_WIDTH);
    }
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

  Window winFrame = XCreateSimpleWindow(
    wm.display,
    wm.root,
    attribs.x,
    attribs.y,
    attribs.width,
    attribs.height,
    BORDER_WIDTH,
    BORDER_COLOR,
    BG_COLOR
  );

  XSelectInput(
    wm.display,
    winFrame,
    SubstructureRedirectMask |
    SubstructureNotifyMask
  );

  XAddToSaveSet(wm.display, win);
  XReparentWindow(wm.display, win, winFrame, 0, 0);

  XMapWindow(wm.display, winFrame);

  wm.client_windows[wm.clients_count].frame = winFrame;
  wm.client_windows[wm.clients_count].win = win;
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
