#include <X11/Xlib.h>
#ifndef OPENPHOTO_H
#define OPENPHOTO_H

typedef struct File {
  char *extension;
  char *name;
} File;

typedef struct XWindow {
  Display *dpy;
  Window w;
  GC gc;
  Visual *visual;
  int depth;
} XWindow ;

typedef struct image_opts {
    int width;
    int height;
    float zoom;
  } image_opts;

#endif
