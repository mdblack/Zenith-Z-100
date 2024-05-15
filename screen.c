// This module implements the Z-100 screen using the gtk graphics library
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "screen.h"
#include "mainboard.h"

// get access to external types define elsewhere
// **DO WE NEED THESE?? SHOULD THEY NOT COME FROM THE MAINBOARD DEFINITIONS via
// A HEADER FILE?
extern unsigned int* pixels;
Z100* z100;

//void renderScreen(Video*, unsigned int*);

const int X_SCALE = 1;
const int Y_SCALE = 2;

GtkWidget *window;
GtkWidget *drawingArea;

// gtk function to draw in gtk window
void display() {
  gtk_widget_queue_draw(window);
}

static gboolean on_keypress(GtkWidget* widget, GdkEventKey* event) {
  // the 'return' char will be used for any incoming keycode that does not
  // correspond to a standard ascii digit or letter - (set the default key code
  // to '\r')
  char code = 0x00;
  /* the GdkEventKey* event is a keyboard hardware event that is detected
    from the user machine. What gets used here is the event->keyval instance
    variable of the GdkEventKey object.
    "keyval" is either one of two things - an ascii character or a keycode */
  printf("*key pressed* GdkEventKey Code: %x\n", event->keyval);

  //handle CONTROL key press
  if(event->keyval == 0xffe3)
  {
    code=0x03;
  }

  // handle BACKSPACE key press
  if(event->keyval == 0xff08) {
    code = 0x08;
  }
  // handle TAB key press
  else if(event->keyval == 0xff09) {
    code = 0x09;
  }
  // handle RETURN key press
  else if(event->keyval == 0xff0d) {
    code = 0x0d;
  }
  // handle ESC key press
  else if(event->keyval == 0xff1b) {
    code = 0x1b;
  }
  // handle DEL key press
  else if(event->keyval == 0xffff) {
    code = 0x7f;
  }
  // handle ENTER (keypad) key press
  else if(event->keyval == 0xff8d) {
    code = (char)0x8d;
  }
  // F0 NOT implemented
//F1-F12
else if (event->keyval == 0xffbe) {
	code=(char)0x97;
}
else if (event->keyval == 0xffbf) {
	code=(char)0x98;
}
else if (event->keyval == 0xffc0) {
	code=(char)0x99;
}
else if (event->keyval == 0xffc1) {
	code=(char)0x9a;
}
else if (event->keyval == 0xffc2) {
	code=(char)0x9b;
}
else if (event->keyval == 0xffc3) {
	code=(char)0x9c;
}
else if (event->keyval == 0xffc4) {
	code=(char)0x9d;
}
else if (event->keyval == 0xffc5) {
	code=(char)0x9e;
}
else if (event->keyval == 0xffc6) {
	code=(char)0x9f;
}
else if (event->keyval == 0xffc7) {
	code=(char)0xa0;
}
else if (event->keyval == 0xffc8) {
	code=(char)0xa1;
}
else if (event->keyval == 0xffc9) {
	code=(char)0xa2;
}
  // handle UP key press
  else if(event->keyval == 0xff52) {
    code = (char)0xa5;
//substitute ctrl-k
//code=(char)0xb;
  }
  // handle DOWN key press
  else if(event->keyval == 0xff54) {
    code = (char)0xa6;
//substitute ctrl-j
//code=(char)0xa;
  }
  // handle RIGHT key press
  else if(event->keyval == 0xff53) {
    code = (char)0xa7;
//substitute ctrl-l
//code=(char)0xc;
  }
  // handle LEFT key press
  else if(event->keyval == 0xff51) {
    code = (char)0xa8;
//substitute ctrl-h
//code=(char)0x8;
  }
  // handle HOME (numLock off-keypad 7 (Home)) key press
  else if(event->keyval == 0xff95) {
    code = (char)0xa9;
  }
  // handle BREAK key press
  else if(event->keyval == 0xff13) {
    code = (char)0xaa;
  }
  // handle keypad '-' '.' key press
  else if(event->keyval == 0xffad || event->keyval == 0xffae ||
    (event->keyval >= 0xffb0 && event->keyval <= 0xffb9)) {
    code = event->keyval & 0xFF;
  }
  // handle all other keys where event->keyval = z-100 keycodes
  else if(event->keyval >= 0x20 && event->keyval <= 0x7E) {
    code = event->keyval;
  }
  // call function keyaction in keyboard.c, which loads the keyboard buffer
  // with the pressed key code
  if(code!=0)
	  keyaction(z100->keyboard, code);
}

static gboolean on_draw_event(GtkWidget* widget, cairo_t *cairo_obj) {
  // holds 24-bit color from pixel array element
  unsigned int p24BitColor;
  // chars to hold each RGB color value
  unsigned char red_val;
  unsigned char green_val;
  unsigned char blue_val;
  /* populate the pixel array using the render screen function defined in video.c.
    This function reads the VRAM and sets up the pixel array accordingly */
  renderScreen(z100->video,pixels);
  // now, cycle through the pixel array as if reading rows/columns
  // loop through rows
  for(int row = 0; row < 225; row++) {
    // loop through columns
    for(int column = 0; column < 640; column++) {
      // get 24-bit colour from pixel array element
      p24BitColor = pixels[(row*640) + column];
      // extract each color component from the 24-bit color
      red_val = (p24BitColor>>16)&0xff;
      green_val = (p24BitColor>>8)&0xff;
      blue_val = p24BitColor&0xff;
      // source RGB data to cairo rectangle
      cairo_set_source_rgb(cairo_obj, red_val, green_val, blue_val);
      // make rectangle (obj, x-coor of left side, y-coor of top, width, height)
      cairo_rectangle(cairo_obj, column*X_SCALE, row*Y_SCALE, X_SCALE, Y_SCALE);
      // color rectangle
      cairo_fill(cairo_obj);
    }
  }
  return FALSE;
}

void screenInit(int* argc, char** argv[]) {

  gtk_init(argc, argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  drawingArea = gtk_drawing_area_new();
  // add drawing area to window
  gtk_container_add(GTK_CONTAINER(window), drawingArea);

  gtk_window_set_title(GTK_WINDOW(window), "Z-100 Screen");
  gtk_window_set_default_size(GTK_WINDOW(window), 640*X_SCALE, 225*Y_SCALE);

  // connect callback functions to GTK window and drawing area default operations
  /* i.e. - when gtk_widget_queue_draw() is called via the display() function called
    from the main Z-100 loop in mainBoard.c, it qualifies as a "draw" event */
  g_signal_connect(G_OBJECT(drawingArea), "draw", G_CALLBACK(on_draw_event), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_keypress), NULL);

  gtk_widget_show_all(window);
}

void screenSetComputer(Z100* c)
{
	z100=c;
}

void screenLoop() {
  // keep GTK window open - will not return
  gtk_main();
}
