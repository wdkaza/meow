#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define FOCUS_BORDER 4
#define UNFOCUS_BORDER 1
#define FOCUS_COLOR 0xff0000
#define UNFOCUS_COLOR 0x0000ff

typedef struct {
  float x, y;
} Vec2;

typedef struct Node{
  Window client;
  int x;
  int y;
  int width;
  int height;
  bool is_focused;
  bool is_mapped;
  bool is_floating;

  char *title;
  char *class_name;
  char *instance_name;

  struct Node *next;
} Node;

typedef enum {
  OP_NONE,
  OP_MOVE,
  OP_RESIZE
} Operation;

typedef struct {
  Display* display;
  Window root;

  bool running;

  Vec2 cursor_start_pos;
  Vec2 cursor_start_frame_pos;
  Vec2 cursor_start_frame_size;

  Atom ATOM_WM_DELETE_WINDOW;
  Atom ATOM_WM_PROTOCOLS;

} XWM;

void handleConfigureNotify(XEvent *ev);
void handleConfigureRequest(XEvent *ev);
void handleMapRequest(XEvent *ev);
void handleUnMapNotify(XEvent *ev);
void handleDestroyNotify(XEvent *ev);
void handleMotionNotify(XEvent *ev);
void handleButtonPress(XEvent *ev);
void handleButtonRelease(XEvent *ev);
void handleClientMessage(XEvent *ev);
void handleKeyPress(XEvent *ev);
void spawn_kitty();
void grabKey(char *key, unsigned int mod);
Node *create_node(Window client);
void delete_node(Node **head, Window client);
Node *findWindowByID(Node *head, Window id);
void focus_window(Node *node);
char *get_window_title(Window client);
void get_window_class(Window client, char **instance, char **class);
void tile_windows();
void send_client_message(Window w, Atom a, long data);

static XWM wm;
int screen_width;
int screen_height;
Node *window_list = NULL;
Operation current_op = OP_NONE;
Node *grabbed_node = NULL;

int xwm_error_handler(Display *dpy, XErrorEvent *ee) {
    char buf[1024];
    XGetErrorText(dpy, ee->error_code, buf, sizeof(buf));
    fprintf(stderr, "X Error: %s (Request: %d)\n", buf, ee->request_code);
    return 0;
}

XWM xwm_init(){
  XWM wm;
  XSetErrorHandler(xwm_error_handler);

  wm.display = XOpenDisplay(NULL);
  if(!wm.display){
    printf("failed to open x display.\n");
    exit(1);
  }
  int screen = DefaultScreen(wm.display);
  screen_width = DisplayWidth(wm.display, screen);
  screen_height = DisplayHeight(wm.display, screen);
  wm.root = DefaultRootWindow(wm.display);
  return wm;
}

void xwm_terminate(){
  XCloseDisplay(wm.display);
}

void xwm_run(){
  wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y =0.0f};
  wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
  wm.running = true;

  XSelectInput(wm.display, wm.root, 
    SubstructureRedirectMask | SubstructureNotifyMask | 
    KeyPressMask | ButtonPressMask | ButtonReleaseMask |
    PointerMotionMask);
  XSync(wm.display, false);

  wm.ATOM_WM_DELETE_WINDOW = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
  wm.ATOM_WM_PROTOCOLS = XInternAtom(wm.display, "WM_PROTOCOLS", false);

  Cursor cursor = XCreateFontCursor(wm.display, XC_left_ptr);
  XDefineCursor(wm.display, wm.root, cursor);
  XSync(wm.display, False);

  // grab keys
  grabKey("Return", Mod4Mask); //mod4+enter to spawn terminal
  grabKey("q", Mod4Mask); //mod4+q to close window

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
      case DestroyNotify:
        handleDestroyNotify(&ev);
        break;
      case KeyPress:
        handleKeyPress(&ev);
        break;
      case UnmapNotify:
        handleUnMapNotify(&ev);
        break;
      case ConfigureNotify:
        handleConfigureNotify(&ev);
        break;
      case ConfigureRequest:
        handleConfigureRequest(&ev);
        break;
      case MotionNotify:
        handleMotionNotify(&ev);
        break;
      case ButtonPress:
        handleButtonPress(&ev);
        break;
      case ButtonRelease:
        handleButtonRelease(&ev);
        break;
    }
  }
}

void send_client_message(Window w, Atom a, long data){
  (void)data;
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = w;
  ev.xclient.message_type = wm.ATOM_WM_PROTOCOLS;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = a;
  ev.xclient.data.l[1] = CurrentTime;
  XSendEvent(wm.display, w, false, NoEventMask, &ev);
}


void grabKey(char *key, unsigned int mod){
  KeySym sym = XStringToKeysym(key);
  KeyCode code = XKeysymToKeycode(wm.display, sym);
  XGrabKey(wm.display, code, mod, wm.root, 0, GrabModeAsync, GrabModeAsync);
}

char *get_window_title(Window client){
  Atom wm_name_atom = XInternAtom(wm.display, "_NEW_WM_NAME", false);
  Atom utf8_string_atom = XInternAtom(wm.display, "UTF8_STRING", false);
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;

  // EWMH
  if(XGetWindowProperty(wm.display, client, wm_name_atom, 0, 1024, False, utf8_string_atom, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success){
    char *result = strndup((char *)prop, nitems);
    XFree(prop);
    return result;
  }
  // ICCCM(fallback)
  if(XGetWindowProperty(wm.display, client, XA_WM_NAME, 0, 1024, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success){
    char *result = strndup((char *)prop, nitems);
    XFree(prop);
    return result;
  }
  if(prop) XFree(prop);
  return strdup("unknown");
}

void get_window_class(Window client, char **instance, char **class){
  XClassHint class_hint;
  if(XGetClassHint(wm.display, client, &class_hint)){
    *instance = strdup(class_hint.res_name);
    *class = strdup(class_hint.res_class);
    XFree(class_hint.res_name);
    XFree(class_hint.res_class);
  }
  else{
    *instance = strdup("unknown");
    *class = strdup("unknown");
  }
}

Node *create_node(Window client){
  XWindowAttributes attr;
  if(!XGetWindowAttributes(wm.display, client, &attr)) return NULL;
  if(attr.override_redirect) return NULL;

  Node *node = calloc(1, sizeof(Node));
  node->client = client;

  node->x = attr.x;
  node->y = attr.y;
  node->width = attr.width;
  node->height = attr.height;

  node->is_focused = false;
  node->is_mapped = false;
  node->is_floating = false;

  node->title = get_window_title(client);
  get_window_class(client, &node->instance_name, &node->class_name);
  return node;
}

void delete_node(Node **head, Window client){
  if(!head || !*head) return;
  Node *current = *head;
  Node *prev = NULL;

  while(current != NULL){
    if(current->client == client){
      if(prev == NULL){
        *head = current->next;
      }
      else{
        prev->next = current->next;
      }
      free(current->class_name);
      free(current->title);
      free(current->instance_name);
      free(current);
      current = NULL;
      return;
    }
    prev = current;
    current = current->next;
  }
}

Node *findWindowByID(Node *head, Window id){
  while(head != NULL){
    if(head->client == id){
      return head;
    }
    head = head->next;
  }
  return NULL;
}

void focus_window(Node *node){
  if(!node) return;
  for(Node *current = window_list; current != NULL; current = current->next){
    if(current->is_focused){
      XSetWindowBorderWidth(wm.display, current->client, UNFOCUS_BORDER);
      XSetWindowBorder(wm.display, current->client, UNFOCUS_COLOR);
      current->is_focused = false;
    }
  }
  node->is_focused = true;
  XSetWindowBorderWidth(wm.display, node->client, FOCUS_BORDER);
  XSetWindowBorder(wm.display, node->client, FOCUS_COLOR);
  XSetInputFocus(wm.display, node->client, RevertToPointerRoot, CurrentTime);
  XRaiseWindow(wm.display, node->client);
  XFlush(wm.display);
}
void handleConfigureNotify(XEvent *ev){
  XConfigureEvent *e = &ev->xconfigure;
  Node *node = findWindowByID(window_list, e->window);
  if(node){
    node->x = e->x;
    node->y = e->y;
    node->width = e->width;
    node->height = e->height;
  }
}

void handleConfigureRequest(XEvent *ev){
  XConfigureRequestEvent *e = &ev->xconfigurerequest;
  XWindowChanges changes;

  changes.x = e->x;
  changes.y = e->y;
  changes.width = e->width;
  changes.height = e->height;
  changes.sibling = e->above;
  changes.stack_mode = e->detail;

  XConfigureWindow(wm.display, e->window, e->value_mask, &changes);
  XFlush(wm.display);
}

void handleKeyPress(XEvent *ev){
  KeySym key = XLookupKeysym(&ev->xkey, 0);
  if(key == XStringToKeysym("Return") && (ev->xkey.state & Mod4Mask)){
    spawn_kitty();
  }
  else if(key == XStringToKeysym("q") && (ev->xkey.state & Mod4Mask)){
    Node *focused = NULL;
    for(Node *current = window_list; current; current = current->next){
      if(current->is_focused){
        focused = current;
        break;
      }
    }
    if(focused){
      send_client_message(focused->client, wm.ATOM_WM_DELETE_WINDOW, 0);
    }
  }
}


void handleButtonPress(XEvent *ev){
  XButtonEvent *e = &ev->xbutton;
  Node *node = findWindowByID(window_list, e->window);
  if(!node) return;

  focus_window(node);

  if(e->button == Button1 && (e->state & Mod1Mask)){
    current_op = OP_MOVE;
    grabbed_node = node;
    wm.cursor_start_pos = (Vec2){.x = e->x_root, .y = e->y_root};
    wm.cursor_start_frame_pos = (Vec2){.x = node->x, .y = node->y};

    XGrabPointer(wm.display, node->client, False, PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
  }
}


void handleMapRequest(XEvent *ev){
  Window w = ev->xmaprequest.window;
  XWindowAttributes attr;

  if(!XGetWindowAttributes(wm.display, w, &attr)){
    fprintf(stderr, "failed to get window attributes\n");
    return;
  }

  if(attr.override_redirect){
    XMapWindow(wm.display, w);
    return;
  }

  if(findWindowByID(window_list, w)){
    XMapWindow(wm.display, w);
    return;
  }

  Node *node = create_node(w);
  if(!node) return;

  XSetWMProtocols(wm.display, w, &wm.ATOM_WM_DELETE_WINDOW, 1);
  XSelectInput(wm.display, w, 
    StructureNotifyMask | 
    PropertyChangeMask |
    FocusChangeMask |
    EnterWindowMask |
    ButtonPressMask | 
    ButtonReleaseMask |
    PointerMotionMask);

  XSetWindowBorderWidth(wm.display, w, UNFOCUS_BORDER);
  XSetWindowBorder(wm.display, w, UNFOCUS_COLOR);

  node->is_mapped = true;
  node->next = window_list;
  window_list = node;

  XMapWindow(wm.display, w);
  focus_window(node);

  XFlush(wm.display);
}

void handleUnMapNotify(XEvent *ev){
  Window w = ev->xunmap.window;
  Node *node = findWindowByID(window_list, w);
  if(node){
    node->is_mapped = false;
    //tile_windows();
  }
}

void handleMotionNotify(XEvent *ev){
  if(current_op == OP_NONE || !grabbed_node) return;

  XMotionEvent *e = &ev->xmotion;
  int dx = e->x_root - wm.cursor_start_pos.x;
  int dy = e->y_root - wm.cursor_start_pos.y;

  if(current_op == OP_MOVE){
    int new_x = wm.cursor_start_frame_pos.x + dx;
    int new_y = wm.cursor_start_frame_pos.y + dy;

    new_x = MAX(0, MIN(new_x, screen_width - grabbed_node->width));
    new_y = MAX(0, MIN(new_y, screen_height - grabbed_node->height));

    XMoveWindow(wm.display, grabbed_node->client, new_x, new_y);
    grabbed_node->x = new_x;
    grabbed_node->y = new_y;
  }
  else if(current_op == OP_RESIZE){
    int new_width = MAX(50, wm.cursor_start_frame_size.x + dx);
    int new_height = MAX(50, wm.cursor_start_frame_size.y + dy);

    XResizeWindow(wm.display, grabbed_node->client, new_width, new_height);
    grabbed_node->width = new_width;
    grabbed_node->height = new_height;
  }
}

void handleDestroyNotify(XEvent *ev){
  Window w = ev->xdestroywindow.window;
  delete_node(&window_list, w);
  if(window_list){
    focus_window(window_list);
  }
  //tile_windows();
}

void handleButtonRelease(XEvent *ev){
  XButtonEvent *e = &ev->xbutton;
  if(e->button == Button1 || e->button == Button3){
    if(current_op != OP_NONE){
      XUngrabPointer(wm.display, CurrentTime);
      current_op = OP_NONE;
      grabbed_node = NULL;
    }
  }
}

void handleClientMessage(XEvent *ev){
  XClientMessageEvent *e = &ev->xclient;
  if(e->message_type == wm.ATOM_WM_PROTOCOLS){
    Atom protocol = e->data.l[0];
    if(protocol == wm.ATOM_WM_DELETE_WINDOW){
      Node *node = findWindowByID(window_list, e->window);
      if(node){
        XDestroyWindow(wm.display, e->window);
      }
    }
  }
}

void spawn_kitty(){
  if(fork() == 0){
    const char *args[] = {"kitty", NULL};
    execvp("kitty", (char * const *)args);

    perror("execvp");
    exit(1);
  }
}

int main(){
  wm = xwm_init();
  xwm_run();
  xwm_terminate();
}

