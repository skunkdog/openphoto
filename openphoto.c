#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* #include <png.h> */
#include </usr/local/include/png.h>

#include "openphoto.h"

#define VERSION "0.1"

typedef struct File {
  char *extension;
  char *name;
} File;

XWindow CreateWindow(int width, int height);

int main(int argc, char *argv[]) {

  int GREYSCALE = 0;
  /* Parce options */
  struct File file_opts;
  if (argc == 1 || !strcmp(argv[1], "-h")){
    printf("HELP\n");          /* Add help */
    return 0;
  }
  else {
    file_opts.name = argv[1];
  }

  for (int i = 2; i < argc; i++) {
    if (!strcmp("-g", argv[i])) {
      GREYSCALE = 1;
    }
  }
  
  /* Check if it is png */
  FILE *header = fopen(file_opts.name, "rb");
  if (!header) {
    fprintf(stderr, "Couldn't find the file \"%s\"\n",file_opts.name);
    return 1;
  }
  /* First 8 bytes are nessesary to check file type */
  size_t buffer_size = 8;
  void *buffer = malloc(buffer_size);

  fread(buffer, 1, buffer_size, header);
  int is_png = !png_sig_cmp(buffer, 0, buffer_size);
  if (is_png) {
    file_opts.extension = "png";
  }
  fclose(header);

  /* Work with image */
  /* TODO: add error pointers */
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
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
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
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

  png_get_IHDR(
	       png_ptr,
	       info_ptr,
	       &png_width,
	       &png_height,
	       &bit_depth,
	       &color_type,
	       &interlace_type,
	       &compression_type,
	       &filter_method
	      );

  
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

  if (color_type == PNG_COLOR_TYPE_RGB ||
      color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
  }

  png_read_update_info(png_ptr, info_ptr);

  int width = (int)png_width;
  int height = (int)png_height;

  png_bytep *row_pointers = malloc(
				   (size_t)height * sizeof(*row_pointers)
				  );

  if (!row_pointers) {
    fprintf(stderr,"Couldn't allocate memory\n");
    fclose(file);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return 1;
  }

  png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);

  for (int y = 0; y < height; y++) {
    row_pointers[y] = malloc(rowbytes);

    if (!row_pointers[y]) {
      for (int j = 0; j < y; j++){
	free(row_pointers[j]);
      }
      free(row_pointers);
      fclose(file);
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 1;
    }
  }

  png_read_image(png_ptr, row_pointers);
  fclose(file);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  
  /* Open window */
  XWindow xwindow = CreateWindow(width,height);

  char *image_data = malloc((size_t)height * width * 4); // Why 4
  if (!image_data) {
    XDestroyWindow(xwindow.dpy, xwindow.w);
    XCloseDisplay(xwindow.dpy);
    free(row_pointers);
  }

  XImage *image = XCreateImage(xwindow.dpy, xwindow.visual, xwindow.depth,
                               ZPixmap, 0, image_data, width, height, 32, 0);
  
  if (!image) {
    fprintf(stderr,"X failed to create XImage\n");
    free(image_data);
    XDestroyWindow(xwindow.dpy, xwindow.w);
    XCloseDisplay(xwindow.dpy);
    free(row_pointers);
    return 1;
  }

  /*                   x → */
  /*          0       1       2       3       4       5       6       7       8       9 */
  /*      ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┐ */
  /* y=0  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,0   │ 1,0   │ 2,0   │ 3,0   │ 4,0   │ 5,0   │ 6,0   │ 7,0   │ 8,0   │ 9,0   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=1  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,1   │ 1,1   │ 2,1   │ 3,1   │ 4,1   │ 5,1   │ 6,1   │ 7,1   │ 8,1   │ 9,1   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=2  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,2   │ 1,2   │ 2,2   │ 3,2   │ 4,2   │ 5,2   │ 6,2   │ 7,2   │ 8,2   │ 9,2   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=3  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,3   │ 1,3   │ 2,3   │ 3,3   │ 4,3   │ 5,3   │ 6,3   │ 7,3   │ 8,3   │ 9,3   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=4  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,4   │ 1,4   │ 2,4   │ 3,4   │ 4,4   │ 5,4   │ 6,4   │ 7,4   │ 8,4   │ 9,4   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=5  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,5   │ 1,5   │ 2,5   │ 3,5   │ 4,5   │ 5,5   │ 6,5   │ 7,5   │ 8,5   │ 9,5   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=6  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,6   │ 1,6   │ 2,6   │ 3,6   │ 4,6   │ 5,6   │ 6,6   │ 7,6   │ 8,6   │ 9,6   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=7  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,7   │ 1,7   │ 2,7   │ 3,7   │ 4,7   │ 5,7   │ 6,7   │ 7,7   │ 8,7   │ 9,7   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=8  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,8   │ 1,8   │ 2,8   │ 3,8   │ 4,8   │ 5,8   │ 6,8   │ 7,8   │ 8,8   │ 9,8   │ */
  /*      ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤ */
  /* y=9  │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ pixel │ */
  /*      │ 0,9   │ 1,9   │ 2,9   │ 3,9   │ 4,9   │ 5,9   │ 6,9   │ 7,9   │ 8,9   │ 9,9   │ */
  /*      └───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┘ */

  
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      png_bytep p = &row_pointers[y][x * 4]; 
      unsigned long red   = p[0];
      unsigned long green = p[1];
      unsigned long blue  = p[2];
      unsigned long pixel = 0;
      
      if (GREYSCALE){
	unsigned long gray = (299 * red + 587 * green + 114 * blue) / 1000;
	pixel |= (gray  * xwindow.visual->red_mask)   / 255;
	pixel |= (gray * xwindow.visual->green_mask) / 255;
        pixel |= (gray * xwindow.visual->blue_mask) / 255;
      }
      
      else {
	pixel |= (red  * xwindow.visual->red_mask)   / 255;
	pixel |= (green * xwindow.visual->green_mask) / 255;
        pixel |= (blue * xwindow.visual->blue_mask) / 255;
      }
      
      XPutPixel(image, x, y, pixel);
    }
  }
  free(row_pointers);
  
  int running = 1;
  while (running) {
    XEvent event;
    XNextEvent(xwindow.dpy, &event);

    switch (event.type) {
    case Expose: {
      XPutImage(
                xwindow.dpy,
                xwindow.w,
		xwindow.gc,
                image,
                0, 0,
                0, 0,
                width, height
	       );
      XFlush(xwindow.dpy);
    }
    case KeyPress: {
      KeySym keysym = XLookupKeysym(&event.xkey, 0);
      if (keysym == XK_q)
        return 0;
    }
    }
  }
  XDestroyWindow(xwindow.dpy, xwindow.w);
  XCloseDisplay(xwindow.dpy);
  
  return 0;
}

XWindow CreateWindow(int width, int height){
  /* Make window */
  Display *dpy = XOpenDisplay(NULL);
  int screen = XDefaultScreen(dpy);
  Visual *visual = DefaultVisual(dpy, screen);
  int depth = DefaultDepth(dpy, screen);
  
  unsigned long white = XWhitePixel(dpy, screen);
  unsigned long black = XBlackPixel(dpy, screen);


  Window w = XCreateSimpleWindow(dpy, XDefaultRootWindow(dpy), 0, 0, width,
                                 height, 2, white, black);
  
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

