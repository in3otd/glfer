/* g_scope.c

 * Copyright (C) 2005-2008 Claudio Girardi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include "glfer.h"
#include "g_scope.h"
#include "fft.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif

#define SCOPE_DA_WIDTH 1024

extern opt_t opt;
extern glfer_t glfer;

static GtkWidget *scope_da;
static GdkPixmap *scope_pixmap = NULL;

static guchar *rgbbuf = NULL;
static int window_width, window_height;
static int pixmap_width, pixmap_height, pixmap_ofs;

static int autoscale = 0;
static int scope_source = 0;

input_source_t input_sources[] = {
  {"/Audio", AUDIO_SOURCE},
  {"/FFT", FFT_SOURCE}
};


/* function prototypes */


static gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  glfer.scope_window = NULL;
  return FALSE;
}

/* copy scope_pixmap content onto scope_da on exposure */
static gboolean scope_da_expose_event(GtkWidget * widget, GdkEventExpose * event, gpointer user_data)
{
  /* copy the relevant portion of the pixmap onto the screen */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], scope_pixmap, event->area.x, pixmap_ofs + event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

//  g_print("expose event! x=%i y=%i w=%i h=%i pixmap_ofs=%i\n", event->area.x, event->area.y, event->area.width, event->area.height, pixmap_ofs);
  return FALSE;
}


static int scope_da_configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer user_data)
{
  if (scope_pixmap) {
    gdk_drawable_unref(scope_pixmap);
  }

  /* create a new pixmap */
  scope_pixmap = gdk_pixmap_new(widget->window, SCOPE_DA_WIDTH, pixmap_height, -1);
  /* clear the new pixmap */
  gdk_draw_rectangle(scope_pixmap, widget->style->fg_gc[GTK_WIDGET_STATE(scope_da)], TRUE, 0, 0, SCOPE_DA_WIDTH, pixmap_height);

  return TRUE;
}


static void input_source_sel_changed(gpointer dummy, guint callback_action, GtkWidget * widget)
{
  scope_source = callback_action; /* 0 = Audio, 1 = FFT */
}


static void autoscale_callback (GtkWidget* widget, gpointer data)
{
  autoscale = TRUE;
}


void scope_window_init(GtkWidget * widget, gpointer data)
{
  gint i;
  GtkWidget *vbox;
  GtkWidget *temp_la, *tmp_hb, *tmp_bu;

  GtkWidget *window = glfer.scope_window;

  GtkItemFactory *item_factory = NULL;
  GtkItemFactoryEntry entry;
  char *menu_title = "<Palette_Types>";
  GtkWidget *menu = NULL;
  GtkWidget *option_menu = NULL;
  
  if (window == NULL) {
    pixmap_height = 300;
    pixmap_width = 300;

    /* scope window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    window_width = 400;
    window_height = 300;
    gtk_window_set_default_size(GTK_WINDOW(window), window_width, window_height);
    gtk_window_set_title(GTK_WINDOW(window), "Oscilloscope");

    g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);

    /* Create a vbox */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);

    /* time-domain signal drawing area */
    scope_da = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(scope_da), SCOPE_DA_WIDTH, pixmap_height);
    g_signal_connect(G_OBJECT(scope_da), "expose_event", G_CALLBACK(scope_da_expose_event), NULL);
    g_signal_connect(G_OBJECT(scope_da), "configure_event", G_CALLBACK(scope_da_configure_event), NULL);
    gtk_widget_set_events(scope_da, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

    gtk_box_pack_start(GTK_BOX(vbox), scope_da, FALSE, FALSE, 1);

    gtk_widget_show(scope_da);

    /* input source selection */
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, menu_title, NULL);
    for (i = 0; i < 2; i++) {
      entry.path = input_sources[i].name;
      entry.accelerator = NULL;
      entry.callback = input_source_sel_changed;
      entry.callback_action = input_sources[i].type;;
      entry.item_type = NULL;
      gtk_item_factory_create_item(item_factory, &entry, NULL, 1);
    }
    menu = gtk_item_factory_get_widget(item_factory, menu_title);
    /* create the option menu for the input source list */
    option_menu = gtk_option_menu_new();
    //gtk_tooltips_set_tip(glfer.tt, option_menu, "Input source for oscilloscope", NULL);
    /* provide the popup menu */
    gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
    /* add label */
    temp_la = gtk_label_new("Input :");
    gtk_widget_show(temp_la);
    tmp_hb = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(tmp_hb);
    gtk_box_pack_start(GTK_BOX(vbox), tmp_hb, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(tmp_hb), temp_la, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(tmp_hb), option_menu, FALSE, FALSE, 0);
    gtk_widget_show(option_menu);

    tmp_bu = gtk_button_new_with_label("Autoscale");
    gtk_box_pack_start(GTK_BOX(tmp_hb), tmp_bu, FALSE, FALSE, 10);
    g_signal_connect(G_OBJECT(tmp_bu), "clicked", G_CALLBACK(autoscale_callback), NULL);
    gtk_widget_show(tmp_bu);
    
    gtk_widget_show(window);
  }

  glfer.scope_window = window;
}


/* draw the data onto the pixmap */
void scope_window_draw(fft_params_t params)
{
  GdkRectangle update_rect;
  double *data, datamax, y_gain;
  int i;
  int N = params.n;
  int y1, y2;

  if (scope_source == 0) 
    data = params.inbuf_audio;
  else
    data = params.inbuf_fft;

  if (autoscale) {
    /* get maximum data value */
    datamax = 0;
    for (i = 0; i < N; i++) {
      if (fabs(data[i]) > datamax) datamax = fabs(data[i]);
    }
    /* make the waveform fill the pixmap */
    if (datamax < 1e-3) datamax = 1e-3; /* avoid division by zero */
    y_gain = (pixmap_height / 2) / datamax;
  } else {
    /* fixed gain */
    y_gain = 20;
  }

  /* clear the spectrum amplitude pixmap */
  gdk_draw_rectangle(scope_pixmap, scope_da->style->fg_gc[GTK_WIDGET_STATE(scope_da)], TRUE, 0, 0, SCOPE_DA_WIDTH, pixmap_height);


  /* i is the pixmap y coordinate */
  for (i = 0; i < N; i++) {
    y1 = pixmap_height / 2 + data[i] * y_gain;
    y2 = pixmap_height / 2 + data[i + 1] * y_gain;
    /* draw spectrum amplitude pixmap */
    //gdk_draw_point(scope_pixmap, scope_da->style->white_gc, i, y);
    gdk_draw_line(scope_pixmap, scope_da->style->white_gc, i, y1, i + 1, y2);
  }
  
  update_rect.x = 0;
  update_rect.width = SCOPE_DA_WIDTH;
  update_rect.y = 0;
  update_rect.height = pixmap_height;

  gtk_widget_draw(scope_da, &update_rect);
}


void scope_window_close()
{
  glfer.scope_window = NULL;
}


