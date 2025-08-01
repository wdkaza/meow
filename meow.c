#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

// thanks @cococry for some of the code
// @cococry also insipered me to try to code this 

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

#define BORDER_WIDTH 3
#define BORDER_COLOR 0xff0000
#define BG_COLOR 0x0000ff

typedef struct {
  float x, y;
} Vec2;

typedef struct {
  Window win;
  Window frame;
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

  Atom ATOM_WM_DELETE_WINDOW;
  Atom ATOM_WM_PROTOCOLS;
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
void sendClientMessage(Window w, Atom a);
Window getFrameWindow(Window w);
bool clientFrameExists(Window w);
bool clientWindowExists(Window w);
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

  XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(wm.display, false);

  wm.clients_count = 0;
  wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
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

  if((e->state & Mod1Mask) && e->button == Button1){
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

  if((e->state & Mod1Mask) && (e->state & Button1Mask)){
    XMoveWindow(wm.display, frame, 
                (int)(wm.cursor_start_frame_pos.x) + dx, 
                (int)(wm.cursor_start_frame_pos.y) + dy
    );
  }
  else if((e->state & Mod1Mask) && (e->state & Button3Mask)){
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
  fprintf(stderr,
    "DEBUG: ConfigureRequest: client=0x%lx x=%d y=%d w=%d h=%d\n",
    e->window, e->x, e->y, e->width, e->height);
  if(!clientFrameExists(e->window)) return;
  Window frame = getFrameWindow(e->window);
  XMoveResizeWindow(wm.display, frame, e->x, e->y, e->width, e->height);
}

void handleKeyPress(XEvent *ev){
  XKeyEvent *e = &ev->xkey;
  if(e->state & Mod1Mask && e->keycode == XKeysymToKeycode(wm.display, XK_Q)){
    Atom* protocols;
    int32_t num_protocols;
    if(XGetWMProtocols(wm.display, e->window, &protocols, &num_protocols) 
      && findAtomPtrRange(protocols, protocols + num_protocols, wm.ATOM_WM_DELETE_WINDOW) != protocols + num_protocols){
      sendClientMessage(e->window, *protocols);
    }
    else{
      XKillClient(wm.display, e->window);
    }
  }
  if(e->state & Mod1Mask && e->keycode == XKeysymToKeycode(wm.display, XK_K)){
    system("kitty &");
  }
}

void handleKeyRelease(XEvent *ev){
  (void)ev;
}

void sendClientMessage(Window w, Atom a){
  XEvent msg = {0};
  msg.xclient.type = ClientMessage;
  msg.xclient.window = w;
  msg.xclient.message_type = wm.ATOM_WM_PROTOCOLS;
  msg.xclient.format = 32;
  msg.xclient.data.l[0] = a;
  XSendEvent(wm.display, w, false, NoEventMask, &msg);
}

void handleMapRequest(XEvent *ev){
  XMapRequestEvent *e = &ev->xmaprequest;
  xwmWindowFrame(e->window, false);
  XMapWindow(wm.display, e->window);
}

void handleUnmapNotify(XEvent *ev){
  XUnmapEvent *e = &ev->xunmap;
  if(!clientWindowExists(e->window) || e->event == wm.root){
    printf("ignoring unmap, not a client window\n");
    return;
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
      break;
    }
  }
}



Atom* findAtomPtrRange(Atom* ptr1, Atom* ptr2, Atom val){
  while(ptr1 < ptr2){
    ptr1++;
    if(*ptr1 == val){
      return ptr1;
    }
  }
  return ptr1;
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

  XGrabButton(wm.display, Button1, Mod1Mask, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.display, Button3, Mod1Mask, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, XK_Q), Mod1Mask, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, XK_K), Mod1Mask, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, XK_Tab), Mod1Mask, win, false, GrabModeAsync, GrabModeAsync);
}

int main(void){
  wm = xwm_init();
  xwm_run();
  return 0;
}
