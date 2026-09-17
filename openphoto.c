#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* #include <png.h> */
#include </usr/local/include/png.h>
/* #include </usr/local/include/spng.h> */

#include "openphoto.h"

#define VERSION "0.1"

XWindow openphoto_create_window(int width, int height) {
  /* Make window */
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr,"X failed to open display.\n");
  }
  int screen = XDefaultScreen(dpy);
  Visual *visual = DefaultVisual(dpy, screen);
  int depth = DefaultDepth(dpy, screen);

  unsigned long white = XWhitePixel(dpy, screen);
  unsigned long black = XBlackPixel(dpy, screen);

  Window w = XCreateSimpleWindow(dpy, XDefaultRootWindow(dpy), 0, 0, width,
                                 height, 2, black, white);

  Atom dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);

  Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);

  XChangeProperty(dpy, w, net_wm_window_type, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&dialog, 1);

  XSelectInput(dpy, w,
               ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | StructureNotifyMask);

  XMapWindow(dpy, w);
  XFlush(dpy);

  GC gc = XCreateGC(dpy, w, 0, NULL);
  XSetForeground(dpy, gc, white);
  XSetLineAttributes(dpy, gc, 3, LineSolid, CapRound, JoinRound);

  XWindow xwindow;
  xwindow.dpy = dpy;
  xwindow.gc = gc;
  xwindow.w = w;
  xwindow.visual = visual;
  xwindow.depth = depth;
  return xwindow;
}

int main(int argc, char *argv[]) {

  /* Options variables */
  int GRAYSCALE = 0;

  /* Print help*/
  struct File file_opts;
  if (argc == 1 || !strcmp(argv[1], "-h")) {
    printf("HELP\n"); /* Add help */
    return 0;
  } else {
    file_opts.name = argv[1];
  }

  /* Parce options */
  for (int i = 2; i < argc; i++) {
    if (!strcmp("-g", argv[i])) {
      GRAYSCALE = 1;
    }
  }

  /* Check if it is png */
  FILE *header = fopen(file_opts.name, "rb");
  if (!header) {
    fprintf(stderr, "Couldn't find the file \"%s\"\n", file_opts.name);
    return 1;
  }
  size_t buffer_size = 8;
  void *buffer = malloc(buffer_size);
  fread(buffer, 1, buffer_size, header);
  int is_png = !png_sig_cmp(buffer, 0, buffer_size);
  if (is_png) {
    file_opts.extension = "png";
  } else {
    fprintf(stderr,"Only png supported yet\n");
  }
  
  free(buffer);
  fclose(header);

  /* Work with image */
  /* TODO: add error pointers */
  png_structp png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    fprintf(stderr, "Failed to create read struct!\n");
    return 1;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    fprintf(stderr, "Failed to create info struct!\n");
    return 1;
  }

  png_infop end_info = png_create_info_struct(png_ptr);
  if (!end_info) {
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fprintf(stderr, "Failed to create info struct!\n");
    return 1;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    fprintf(stderr, "Libpng error while reading PNG\n");
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return 1;
  }

  /* Open the file */
  FILE *file = fopen(file_opts.name, "rb");
  png_init_io(png_ptr, file);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 png_width;
  png_uint_32 png_height;
  int bit_depth;
  int color_type;
  int interlace_type;
  int compression_type;
  int filter_method;

  png_get_IHDR(png_ptr, info_ptr, &png_width, &png_height, &bit_depth,
               &color_type, &interlace_type, &compression_type, &filter_method);

  /* Image options */
  image_opts image_opts;
  image_opts.width = (int)png_width;
  image_opts.height = (int)png_height;
  image_opts.zoom = 1.0;

  /* Open window */
  XWindow xwindow =
      openphoto_create_window(image_opts.width, image_opts.height);
  /*
   * Convert all supported PNG types to 8-bit RGBA.
   */

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  if ((color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png_ptr);

  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
  }

  png_read_update_info(png_ptr, info_ptr);
  png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);

  int height = image_opts.height;
  
  png_bytep pixels = malloc(rowbytes * (size_t)height);

  if (!pixels) {
    fprintf(stderr, "Couldn't allocate image buffer\n");
    fclose(file);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return 1;
  }

  /*
   * Read each PNG row directly into the correct location.
   */
  for (int y = 0; y < height; y++) {
    png_read_row(png_ptr, pixels + ((size_t)y * rowbytes), NULL);
  }

  /*
   * Optional but recommended: finish reading the PNG.
   */
  png_read_end(png_ptr, NULL);

  fclose(file);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  fclose(file);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  char *image_data =
      malloc((size_t)image_opts.height * (size_t)image_opts.width * 4);
  
  if (!image_data) {
    fprintf(stderr, "Couldn't allocate XImage data\n");

    free(pixels);
    XDestroyWindow(xwindow.dpy, xwindow.w);
    XFreeGC(xwindow.dpy, xwindow.gc);
    XCloseDisplay(xwindow.dpy);
    return 1;
}

  XImage *image =
      XCreateImage(xwindow.dpy, xwindow.visual, xwindow.depth, ZPixmap, 0,
                   image_data, image_opts.width, image_opts.height, 32, 0);

  if (!image) {
    fprintf(stderr, "X failed to create XImage\n");
    free(image_data);
    XDestroyWindow(xwindow.dpy, xwindow.w);
    XCloseDisplay(xwindow.dpy);
    return 1;
  }

  unsigned int *dest = (unsigned int *)image->data;

  for (int y = 0; y < image_opts.height; y++) {
    for (int x = 0; x < image_opts.width; x++) {

      png_bytep p = pixels + ((size_t)y * rowbytes) + ((size_t)x * 4);
      unsigned long red = p[0];
      unsigned long green = p[1];
      unsigned long blue = p[2];
      unsigned long pixel = 0;

      if (GRAYSCALE) {
        unsigned long gray = (299 * red + 587 * green + 114 * blue) / 1000;
        pixel |= (gray * xwindow.visual->red_mask) / 255;
        pixel |= (gray * xwindow.visual->green_mask) / 255;
        pixel |= (gray * xwindow.visual->blue_mask) / 255;
      }

      else {
        pixel |= (red * xwindow.visual->red_mask) / 255;
        pixel |= (green * xwindow.visual->green_mask) / 255;
        pixel |= (blue * xwindow.visual->blue_mask) / 255;
      }

      dest[y * (image->bytes_per_line / 4) + x] = pixel;
    }
  }
  free(pixels);

  Pixmap pixmap = XCreatePixmap(xwindow.dpy, xwindow.w, image_opts.width,
                                image_opts.height, xwindow.depth);

  XPutImage(xwindow.dpy, pixmap, xwindow.gc, image, 0, 0, 0, 0,
            image_opts.width, image_opts.height);

  int running = 1;
  while (running) {
    XEvent event;
    XNextEvent(xwindow.dpy, &event);

    switch (event.type) {
    case Expose: {

      XCopyArea(xwindow.dpy, pixmap, xwindow.w, xwindow.gc, 0, 0,
                image_opts.width * image_opts.zoom,
                image_opts.height * image_opts.zoom, 0, 0);

      XFlush(xwindow.dpy);
      break;
    }

    case KeyPress: {
      KeySym keysym = XLookupKeysym(&event.xkey, 0);
      if (keysym == XK_q) {
	running = 0;
      }

      else if (keysym == XK_minus) {
        /* Zoom out */
        image_opts.zoom -= 0.1;
        printf("%f\n", image_opts.zoom);
      }

      else if (keysym == XK_equal) {
        /* Zoom in */
        image_opts.zoom += 0.1;
        printf("%f\n", image_opts.zoom);
      }
      break;
    }
    }
  }
  XDestroyImage(image);
  XFreePixmap(xwindow.dpy, pixmap);
  XFreeGC(xwindow.dpy, xwindow.gc);
  XDestroyWindow(xwindow.dpy, xwindow.w);
  XCloseDisplay(xwindow.dpy);
  return 0;
}

/* TODO: - Zoom */
