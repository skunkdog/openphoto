#include <X11/Xlib.h>
#ifndef OPENPHOTO
#define OPENPHOTO

typedef struct XWindow {
  Display *dpy;
  Window w;
  GC gc;
  Visual *visual;
  int depth;
} XWindow ;


#endif
