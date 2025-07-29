#include <X11/Xlib.h>
#include <unistd.h>
#include <stdlib.h>

// placeholder tinywm for today

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int main(void){
  Display *dpy;
  XWindowAttributes attr;
  XButtonEvent start;
  XEvent ev;

  if(!(dpy = XOpenDisplay(0x0))) return 1;

  XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("F1")), Mod1Mask,
           DefaultRootWindow(dpy), true, GrabModeAsync, GrabModeAsync);
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
