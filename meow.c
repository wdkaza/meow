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


typedef struct {
  float x, y;
} Vec2;

typedef struct Node{
  Window client;
  //int clientID; // dont know if will use
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



Display *dpy;
Window root;
int screen_width;
int screen_heiht;
Node *window_list = NULL;
static Window last_focused_window = None;


void retile();


void panic(char *msg){
  puts(msg);
  exit(EXIT_FAILURE);
}

void grabKey(char *key, unsigned int mod){
  KeySym sym = XStringToKeysym(key);
  KeyCode code = XKeysymToKeycode(dpy, sym);
  XGrabKey(dpy, code, mod, root, 0, GrabModeAsync, GrabModeAsync);
}

char *get_window_title(Window client){
  Atom wm_name_atom = XInternAtom(dpy, "_NEW_WM_NAME", false);
  Atom utf8_string_atom = XInternAtom(dpy, "UTF8_STRING", false);
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;

  // EWMH
  if(XGetWindowProperty(dpy, client, wm_name_atom, 0, 1024, False, utf8_string_atom, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success){
    char *result = strndup((char *)prop, nitems);
    XFree(prop);
    return result;
  }
  // ICCCM(fallback)
  if(XGetWindowProperty(dpy, client, XA_WM_NAME, 0, 1024, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success){
    char *result = strndup((char *)prop, nitems);
    XFree(prop);
    return result;
  }
  if(prop) XFree(prop);
  return strdup("unknown");
}

void get_window_class(Window client, char **instance, char **class){
  XClassHint class_hint;
  if(XGetClassHint(dpy, client, &class_hint)){
    *instance = strdup(class_hint.res_name);
    *class = strdup(class_hint.res_class);
    XFree(class_hint.res_name);
    XFree(class_hint.res_class);
  }
  else{
    *instance = NULL;
    *class = NULL;
  }
}

Node *create_node(Window client){
  XWindowAttributes attr;
  if(!XGetWindowAttributes(dpy, client, &attr)) return NULL;
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
    current->is_focused = false;
  }
  node->is_focused = true;
  XSetInputFocus(dpy, node->client, RevertToPointerRoot, CurrentTime);
  XRaiseWindow(dpy, node->client);
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
  /*
  XConfigureRequestEvent *e = &ev->xconfigurerequest;
  XWindowChanges changes;

  changes.x = e->x;
  changes.y = e->y;
  changes.width = e->width;
  changes.height = e->height;
  changes.sibling = e->above;
  changes.stack_mode = e->detail;

  XConfigureWindow(dpy, e->window, e->value_mask, &changes);
  XSync(dpy, False);
  */
  (void)ev;
}

void handleMapRequest(XEvent *ev){
  Window w = ev->xmaprequest.window;
  if(!findWindowByID(window_list, w)){
    Node *node = create_node(w);
    if(!node) return;
    node->is_mapped = true;
    node->next = window_list;
    window_list = node;
    XMapWindow(dpy, w);
    retile();
    focus_window(node);
  }
}

void handleUnMapNotify(XEvent *ev){
  Window w = ev->xunmap.window;
  Node *node = findWindowByID(window_list, w);
  if(node){
    node->is_mapped = false;
  }
  retile();
}

void handleDestroyNotify(XEvent *ev){
  Window w = ev->xdestroywindow.window;
  delete_node(&window_list, w);
  retile();
}

void handleMotionNotify(XEvent *ev){
  (void)ev;
}

void spawn_kitty(){
  if(fork() == 0){
    const char *args[] = {"kitty", NULL};
    execvp("kitty", (char * const *)args);

    perror("execvp");
    exit(1);
  }
}


int main(void){

  dpy = XOpenDisplay(NULL);
  if(dpy == NULL) panic("unable to open a X display");


  root = DefaultRootWindow(dpy);
  screen_width = DisplayWidth(dpy, DefaultScreen(dpy));
  screen_heiht = DisplayHeight(dpy, DefaultScreen(dpy));

  XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
  XGrabButton(dpy, Button1, 0, root, False, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);

  grabKey("a", ShiftMask);
  XSync(dpy, 0);


  Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
  XDefineCursor(dpy, root, cursor);
  XSync(dpy, False);


  XEvent ev;
  for(;;){
    XNextEvent(dpy, &ev);

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
        spawn_kitty();
        focus_window(window_list);
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
    }

    XSync(dpy, 0);
  }

  XCloseDisplay(dpy);
}



/*
int main(void){
  Display *dpy;
  XWindowAttributes attr;
  XButtonEvent start;
  XEvent ev;

  if(!(dpy = XOpenDisplay(0x0))) return 1;

  XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("F1")), Mod1Mask,
           DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync);
  XGrabButton(dpy, 1, Mod1Mask, DefaultRootWindow(dpy), True,
           ButtonPressMask | ButtonReleaseMask | PointerMotionMask, 
           GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(dpy, 3, Mod1Mask, DefaultRootWindow(dpy), True,
           ButtonPressMask | ButtonReleaseMask | PointerMotionMask, 
           GrabModeAsync, GrabModeAsync, None, None);
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureNotifyMask);
  start.subwindow = None;
  for(;;){
    XNextEvent(dpy, &ev);
    if(ev.type == MapNotify){
      XSetWindowBorderWidth(dpy, ev.xmap.window, 4);
      XSetWindowBorder(dpy, ev.xmap.window, WhitePixel(dpy, DefaultScreen(dpy)));
      XRaiseWindow(dpy, ev.xmap.window);
      XSetInputFocus(dpy, ev.xmap.window, RevertToPointerRoot, CurrentTime);
    }
    if(ev.type == KeyPress && ev.xkey.subwindow != None){
      XRaiseWindow(dpy, ev.xkey.subwindow);
    }
    else if(ev.type == KeyPress){
      if(ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("F1")) && (ev.xkey.state & Mod1Mask)){
        if(fork() == 0){
          execlp("kitty", "kitty", NULL);
          _exit(1);
        }
      }
    }
    else if(ev.type == ButtonPress){
      Window root_return, child_return;
      int root_x, root_y, win_x, win_y;
      unsigned int mask_return;

      if(XQueryPointer(dpy, DefaultRootWindow(dpy),
                    &root_return, &child_return,
                    &root_x, &root_y, &win_x, &win_y,
                    &mask_return) && child_return != None) {
        XGetWindowAttributes(dpy, child_return, &attr);
        start = ev.xbutton;
        start.subwindow = child_return;  // Update start.subwindow to the focused window

        XRaiseWindow(dpy, child_return);
        XSetInputFocus(dpy, child_return, RevertToPointerRoot, CurrentTime);
      }
      else{
        start.subwindow = None;
      }
    }
    else if(ev.type == MotionNotify && start.subwindow != None){
      int xdiff = ev.xbutton.x_root - start.x_root;
      int ydiff = ev.xbutton.y_root - start.y_root;
      XMoveResizeWindow(dpy, start.subwindow,
                        attr.x + (start.button == 1 ? xdiff : 0),
                        attr.y + (start.button == 1 ? ydiff : 0),
                        MAX(1, attr.width + (start.button == 3 ? xdiff : 0)),
                        MAX(1, attr.height + (start.button == 3 ? ydiff : 0)));
    }
    else if(ev.type == ButtonRelease){
      start.subwindow = None;
    }
  }
}
*/
