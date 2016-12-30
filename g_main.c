/* g_main.c

 * Copyright (C) 2001-2008 Claudio Girardi
 *           (C) 2006      Edouard Griffiths for modifications implementing
 *                         averaging mode
 *
 * This file is derived from xspectrum, Copyright (C) 2000 Vincent Arkesteijn
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
#include "g_about.h"
#include "g_txmsg.h"
#include "g_options.h"
#include "g_main.h"
#include "g_scope.h"
#include "g_file_dialogs.h"
#include "util.h"
#include "fft.h"
#include "avg.h"
#include "glfer.h"
#include "source.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif

/* minimum distance between pressing and releasing mouse button for */
/* selection to be acknowledged */
#define MINSEL 5

/* istantaneous spectrum amplitude width */
#define SPEC_DA_WIDTH 200

extern opt_t opt;
extern glfer_t glfer;
extern avg_data_t avgdata;

static GtkWidget *window = NULL, *main_vb, *avg_hb, *drawing_area, *spec_da;
static GtkWidget *scrolled_window, *stop_start_bu, *cpu_la, *pwr_la, *peak_la, *avgmax_frame = NULL, *avgmax_la, *avgvar_frame = NULL, *avgvar_la, *avgfill_frame = NULL, *avgfill_la, *peakfreq_frame = NULL, *peakfreq_la;
static GdkGC *sel_gc = NULL;
static GdkCursor *cursors[2];
static GtkAdjustment *freq_ad;
static GdkPixmap *pixmap = NULL, *spec_pixmap = NULL, *spcols_px = NULL;

static guchar *rgbbuf = NULL;
static short *levbuf = NULL;
static int n, n_zoom = 1, l = 600;
static int window_width, window_height;
static int pixmap_width, pixmap_height, pixmap_ofs;
static float min_freq, max_freq, center_freq;
static gint x_down, y_down;	/* for mouse button press event */
static unsigned char colortab[256 * 3];

static gint sel_width, sel_height;	/* area selected for JPEG save */
GdkPixbuf *pixbuf = NULL; /* pixbuf used for saving spectrum images */

/* received signal strenght values */
float sig_pwr = 0, sig_mean_pwr = 0, floor_pwr = 0, floor_mean_pwr = 0;
static float peak_pwr;
static unsigned int peak_bin;

/* function prototypes */
static void selection_button_press_event(GtkWidget * widget, GdkEventButton * event);
static void get_main_menu(GtkWidget * window, GtkWidget ** menu_bar);
static gint delete_event(GtkWidget * widget, GdkEvent * event, gpointer data);
static gboolean drawing_area_expose_event(GtkWidget * widget, GdkEventExpose * event, gpointer user_data);
static int drawing_area_configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer user_data);
static gdouble da_to_act_y(gint y);
static gboolean drawing_area_motion_event(GtkWidget * widget, GdkEventMotion * event, gpointer data);
static void selection_event(GtkWidget * widget, GdkEventMotion * event, gpointer data);
static gboolean drawing_area_button_press_event(GtkWidget * widget, GdkEventButton * event);
static void select_soundcard_source(GtkWidget * widget, gpointer data);
static void select_mode(GtkWidget * widget, gpointer data);
static void save_full_pixmap(GtkWidget * widget, gpointer data);
static void save_pixmap_region(GtkWidget * widget, gpointer data);
static void spectrum_file_save(gchar *filename);
static void insert_avg_frames(void);
static void remove_avg_frames(void);

static int x_cur;		// EG current horizontal position in the spectrogram window

/*
void jason_mode(GtkWidget * widget, gpointer data)
{
}
*/

static GtkItemFactoryEntry menu_items[] = {
  {"/_Source", NULL, NULL, 0, "<Branch>"},
  {"/Source/_Open file", "<control>O", open_file_dialog, 0, NULL},
  {"/Source/_Sound card", "<control>S", select_soundcard_source, 0, NULL},
  {"/Source/sep1", NULL, NULL, 0, "<Separator>"},
  {"/Source/Quit", "<control>Q", gtk_main_quit, 0, NULL},
  {"/_Mode", NULL, NULL, 0, "<Branch>"},
  {"/_Mode/FFT", NULL, select_mode, MODE_FFT, "<RadioItem>"},
  {"/_Mode/MTM", NULL, select_mode, MODE_MTM, "/Mode/FFT"},
  {"/_Mode/HPARMA", NULL, select_mode, MODE_HPARMA, "/Mode/FFT"},
  {"/_Mode/LMP", NULL, select_mode, MODE_LMP, "/Mode/FFT"},
  {"/Settings", NULL, NULL, 0, "<Branch>"},
  {"/Settings/Spectrum", NULL, spec_settings_dialog, 0, NULL},
  {"/Settings/QRSS", NULL, qrss_settings_dialog, 0, NULL},
  {"/Settings/Port", NULL, port_settings_dialog, 0, NULL},
  {"/Settings/Audio", NULL, audio_settings_dialog, 0, NULL},
  {"/QSO", NULL, NULL, 0, "<Branch>"},
  {"/QSO/Message", NULL, create_txmsg_window, 0, NULL},
/*  {"/Jason", NULL, jason_mode, 0, NULL}, */
  {"/Save", NULL, NULL, 0, "<Branch>"},
  {"/Save/Full Spectrgram", NULL, save_full_pixmap, 0, NULL},
  {"/Save/Select region", NULL, save_pixmap_region, 0, NULL},
  {"/Test", NULL, tx_test_dialog, 0, NULL},
  {"/Scope", NULL, scope_window_init, 0, NULL},
  {"/_Help", NULL, NULL, 0, "<LastBranch>"},
  {"/_Help/About", NULL, show_about, 0, NULL},
  {"/_Help/License", NULL, show_license, 0, NULL},
};


static void get_main_menu(GtkWidget * window, GtkWidget ** menu_bar)
{
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;

  gint nmenu_items = sizeof(menu_items) / sizeof(menu_items[0]);

  accel_group = gtk_accel_group_new();
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);

  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, item_factory);

  /* if startup file not loaded disable transmissions */
  glfer.test_menu_item = gtk_item_factory_get_widget(item_factory, "/Test");
  glfer.qso_menu_item = gtk_item_factory_get_widget(item_factory, "/QSO");
  if (glfer.init_done == FALSE) {
    gtk_widget_set_sensitive(glfer.qso_menu_item, FALSE);
    gtk_widget_set_sensitive(glfer.test_menu_item, FALSE);
  }

  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  if (menu_bar)
    *menu_bar = gtk_item_factory_get_widget(item_factory, "<main>");
}


static gint update_avgmax(gpointer data)	
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;

  if (opt.averaging == AVG_PLAIN)
    g_string_sprintf(buf_1, " %2.1f/%2.1fdB ", 10.0 * log10(glfer.avgmax), 10.0 * log10(glfer.peakval));
  else if (opt.averaging != NO_AVG)
    g_string_sprintf(buf_1, " %2.1fdB ", 10.0 * log10(glfer.avgmax));
  else
    g_string_sprintf(buf_1, " n/a ");
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static gint update_avgvar(gpointer data)	
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;

  if (opt.averaging == AVG_SUMAVG)
    g_string_sprintf(buf_1, " %2.1fdB ", 10.0 * log10(glfer.avgmax / glfer.avgvar));
  else
    g_string_sprintf(buf_1, " n/a ");
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static gint update_avgfill(gpointer data)	
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;

  if (glfer.avgfill < opt.avgsamples)
    g_string_sprintf(buf_1, " %2.1f%% ", 100.0 * ((float) glfer.avgfill / (float) opt.avgsamples));
  else
    g_string_sprintf(buf_1, " 100.0%%");

  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static gint update_peakfreq(gpointer data)	
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;

  if (opt.averaging != NO_AVG)
    g_string_sprintf(buf_1, " %.1fHz ", glfer.peakfreq + opt.offset_freq);
  else
    g_string_sprintf(buf_1, " n/a ");
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static gint update_pwr(gpointer data)
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;
  //sig_mean_pwr   = sig_mean_pwr   * 0.999 + sig_pwr   * 0.001;
  //floor_mean_pwr = floor_mean_pwr * 0.999 + floor_pwr * 0.001;
  sig_mean_pwr = sig_pwr;
  floor_mean_pwr = floor_pwr;

  g_string_sprintf(buf_1, "   S: %5.1lf  N: %5.1lf S/N: %5.1lf  ", 10 * log10(sig_mean_pwr), 10 * log10(floor_mean_pwr), 10 * log(sig_mean_pwr) - 10 * log(floor_mean_pwr));
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static gint update_peak(gpointer data)
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;

  g_string_sprintf(buf_1, " Power: %5.1le at bin %5.1i ", peak_pwr, peak_bin);
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static gint update_cpu_usage(gpointer data)
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);
  static float cpu_usage = 0.0;

  label = (GtkWidget *) data;
  /* filter */
  cpu_usage = 0.1 * glfer.cpu_usage + 0.9 * cpu_usage;
  g_string_sprintf(buf_1, " %.2f%% ", cpu_usage * 100.0);
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* don't stop calling this idle function */
  return TRUE;
}


static void select_soundcard_source(GtkWidget * widget, gpointer data)
{
  stop_reading_audio();
  close_audio(); /* close current audio source (not really needed if we are currently reading from soundcard */
  init_audio(NULL);
  start_reading_audio();
  glfer.first_buffer = TRUE; /* need to reinitialize display autoscale */
}


static gint delete_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
  return FALSE;
}


/* copy spcol_px pixmap content onto spec_da on exposure */
static gboolean cols_da_expose_event(GtkWidget * widget, GdkEventExpose * event, gpointer user_data)
{
  /* copy the relevant portion of the pixmap onto the screen */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], spec_pixmap, event->area.x, pixmap_ofs + event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

  return FALSE;
}


static int spcols_da_configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer user_data)
{
  if (spcols_px) {
    gdk_drawable_unref(spcols_px);
  }

  /* create a new pixmap */
  spcols_px = gdk_pixmap_new(widget->window, SPEC_DA_WIDTH, 20, -1);
  /* clear the new pixmap */
  gdk_draw_rectangle(spcols_px, widget->style->fg_gc[GTK_WIDGET_STATE(drawing_area)], TRUE, 0, 0, SPEC_DA_WIDTH, 20);

  return TRUE;
}


/* copy spec_pixmap content onto spec_da on exposure */
static gboolean spec_da_expose_event(GtkWidget * widget, GdkEventExpose * event, gpointer user_data)
{
  /* copy the relevant portion of the pixmap onto the screen */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], spec_pixmap, event->area.x, pixmap_ofs + event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

//  g_print("expose event! x=%i y=%i w=%i h=%i pixmap_ofs=%i\n", event->area.x, event->area.y, event->area.width, event->area.height, pixmap_ofs);
  return FALSE;
}


static int spec_da_configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer user_data)
{
  if (spec_pixmap) {
    gdk_drawable_unref(spec_pixmap);
  }

  /* create a new pixmap */
  spec_pixmap = gdk_pixmap_new(widget->window, SPEC_DA_WIDTH, pixmap_height, -1);
  /* clear the new pixmap */
  gdk_draw_rectangle(spec_pixmap, widget->style->fg_gc[GTK_WIDGET_STATE(drawing_area)], TRUE, 0, 0, SPEC_DA_WIDTH, pixmap_height);

  return TRUE;
}


/* copy pixmap content onto drawing_area on exposure */
static gboolean drawing_area_expose_event(GtkWidget * widget, GdkEventExpose * event, gpointer user_data)
{
  /* copy the relevant portion of the pixmap onto the screen */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, event->area.x, pixmap_ofs + event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

//  g_print("expose event! x=%i y=%i w=%i h=%i pixmap_ofs=%i\n", event->area.x, event->area.y, event->area.width, event->area.height, pixmap_ofs);
  return FALSE;
}


static int drawing_area_configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer user_data)
{
  GdkPixmap *new_pixmap;
  int new_l, new_n_zoom = 0;

  D(printf("configure event: widget = %p, all_height = %i, all_width = %i\n", widget, widget->allocation.height, widget->allocation.width));	/* for debug */
  D(printf("configure event: event_height = %i, event_width = %i\n", event->height, event->width));	/* for debug */
  /* do lots of stuff when the window changes size */
  /* if the window height isn't an integer multiple of n, set it to one */
  //pixmap_height = event->height;
  //pixmap_width = event->width;
  /* set the pixmap_width/height to the dimensions of the drawing area */
  //pixmap_height = widget->allocation.height;
  pixmap_width = widget->allocation.width;

  //gdk_window_resize(drawing_area->window, pixmap_width, pixmap_height);
  //gtk_widget_set_usize(GTK_WIDGET(drawing_area->window), -1, -1);
  //gtk_widget_set_usize(GTK_WIDGET(drawing_area->window), pixmap_width, pixmap_height);
  //gtk_widget_queue_resize(GTK_WIDGET(drawing_area));


  /* this is the zoom factor needed to fill the entire drawing area */
  //new_n_zoom = pixmap_height / n;
  if (new_n_zoom == 0)
    new_n_zoom = 1;
  /* new drawing area length */
  new_l = pixmap_width;

  /* create a new pixmap */
  new_pixmap = gdk_pixmap_new(widget->window, pixmap_width, pixmap_height, -1);
  /* clear the new pixmap */
  gdk_draw_rectangle(new_pixmap, widget->style->fg_gc[GTK_WIDGET_STATE(drawing_area)], TRUE, 0, 0, pixmap_width, pixmap_height);

  if (pixmap) {
    gdk_draw_drawable(new_pixmap, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, 0, 0, 0, 0, (new_l > l) ? l : new_l, (new_n_zoom > n_zoom) ? n_zoom * n : new_n_zoom * n);
    gdk_drawable_unref(pixmap);
    FREE_MAYBE(rgbbuf);
  }
  pixmap = new_pixmap;
  n_zoom = new_n_zoom;
  l = new_l;
  rgbbuf = calloc(3 * n * n_zoom, sizeof(guchar));

  if (levbuf)
    free(levbuf);
  //fprintf(stderr, "levbuf[%i*%i] size requested = %i\n", pixmap_width, pixmap_height, pixmap_width * pixmap_height * sizeof(short));
  levbuf = calloc(pixmap_width * pixmap_height, sizeof(short));
  if (!levbuf) {
    fprintf(stderr, "Error allocating levbuf");
    exit(-1);
  }

  D(printf("configure event: nzoom = %i, p_height = %i, p_width = %i\n\n", n_zoom, pixmap_height, pixmap_width));	/* for debug */
  return TRUE;
}


/* da_to_act_x, converts the pixmap x-coordinate to its real x-coordinate (time) */
static float da_to_act_x(float x)
{
  float time;
  float time_unit = ((float) opt.data_block_size / (float) opt.sample_rate) * (1.0 - opt.data_blocks_overlap);

  if (x - x_cur < 0)
    time = time_unit * (x - x_cur);
  else
    time = time_unit * (x - x_cur - pixmap_width);

  return time;
}


/* da_to_act_x, converts the pixmap x-coordinate to its real x-coordinate (time) relative to spectrogram window */
static float da_to_act_x_rel(float x)
{
  float time_unit = ((float) opt.data_block_size / (float) opt.sample_rate) * (1.0 - opt.data_blocks_overlap);

  return x * time_unit;
}


/* da_to_act_y, converts the pixmap y-coordinate to its real y-coordinate  */
static gdouble da_to_act_y(gint y)
{
  gdouble freq;

  /* y can go from 0 to (pixmap_height - 1) */
  freq = opt.offset_freq + min_freq + (max_freq - min_freq) * (1.0 - y / (pixmap_height - 1.0));

  return fabs(freq);
}


static gint motion_notify_event(GtkWidget * widget, GdkEventMotion * event)
{
  int x, y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer(event->window, &x, &y, &state);
  else {
    x = event->x;
    y = event->y;
    state = event->state;
  }

//  if (state & GDK_BUTTON1_MASK && pixmap != NULL) draw_brush(widget, x, y);

  /* let event be processed by any further callback */
  return FALSE;
}


static gboolean drawing_area_motion_event(GtkWidget * widget, GdkEventMotion * event, gpointer data)
{
  GtkWidget *label;
  GString *buf_1 = g_string_new(NULL);

  label = (GtkWidget *) data;

  g_string_sprintf(buf_1, "%.3f Hz %.3f/%.3fs %3.2i dB", da_to_act_y(event->y), da_to_act_x(event->x), da_to_act_x_rel(event->x), levbuf[(int) (event->x * pixmap_height + event->y)]);
  gtk_label_set_text(GTK_LABEL(label), buf_1->str);

  g_string_free(buf_1, TRUE);

  /* let event be processed by any further callback */
  return FALSE;
}


/* selection_event, this callback is registered after a mouse button has been
 * pressed in the drawing area, it erases the ?last? rectangle and draws 
 * another to represent the area the user has selected. the callback is
 * unregistered when the user releases the mouse button
 */
static void selection_event(GtkWidget * widget, GdkEventMotion * event, gpointer data)
{
  static gint x_left = 0, y_top = 0;
  static gint x_right = 0, y_bottom = 0;
  gint x, y;

  /* vanish the last selection */
  /* top horizontal line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_left, y_top, x_left, y_top, x_right - x_left, 1);
  /* left vertical line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_left, y_top, x_left, y_top, 1, y_bottom - y_top);
  /* bottom horizontal line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_left, y_bottom, x_left, y_bottom, x_right - x_left + 1, 1);
  /* right vertical line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_right, y_top, x_right, y_top, 1, y_bottom - y_top);

  //gdk_gc_set_foreground(sel_gc, m_colors.sel);
  gdk_gc_set_line_attributes(sel_gc, 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);

  x = (gint) event->x;
  y = (gint) event->y;

  x_left = (x_down < x) ? x_down : x;
  x_right = (x_down > x) ? x_down : x;
  y_top = (y_down < y) ? y_down : y;
  y_bottom = (y_down > y) ? y_down : y;

  gdk_draw_rectangle(widget->window, sel_gc, FALSE, x_left, y_top, x_right - x_left, y_bottom - y_top);
  // fprintf(stderr, "%i %i %i %i\n", x_left, x_right, y_top, y_bottom);
}


/* release_event, event that occurs when the user releases button on the
 * drawing area
 */
static void release_event(GtkWidget * widget, GdkEventButton * event, gpointer data)
{
  gint x_up, y_up;
  gdouble x_left, x_right;
  gdouble y_top, y_bottom;

  /* remove the selection callback, and this callback */
  gtk_signal_disconnect_by_func(GTK_OBJECT(widget), G_CALLBACK(selection_button_press_event), NULL);
  gtk_signal_disconnect_by_func(GTK_OBJECT(widget), G_CALLBACK(release_event), NULL);
  gtk_signal_disconnect_by_func(GTK_OBJECT(widget), G_CALLBACK(selection_event), NULL);

  /* put the normal cursor back */
  gdk_window_set_cursor(widget->window, cursors[0]);

  x_up = (gint) event->x;
  y_up = (gint) event->y;

  /* if the user pressed and released the button nearby, we'll let 'em off */
  if ((abs(x_up - x_down) < MINSEL) && (abs(y_up - y_down) < MINSEL))
    return;

  /* check for zero width or zero height */
  if ((x_up == x_down) || (y_up == y_down))
    return;

  x_left = (x_down < x_up) ? x_down : x_up;
  x_right = (x_down > x_up) ? x_down : x_up;
  y_top = (y_down < y_up) ? y_down : y_up;
  y_bottom = (y_down > y_up) ? y_down : y_up;

  /* vanish the selection */
  /* top horizontal line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_left, y_top, x_left, y_top, x_right - x_left, 1);
  /* left vertical line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_left, y_top, x_left, y_top, 1, y_bottom - y_top);
  /* bottom horizontal line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_left, y_bottom, x_left, y_bottom, x_right - x_left + 1, 1);
  /* right vertical line of rectangle */
  gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, x_right, y_top, x_right, y_top, 1, y_bottom - y_top);

  sel_width = x_right - x_left + 1;
  sel_height = y_bottom - y_top + 1;

  D(printf("x_left = %f, y_top = %f, x_right = %f, y_bottom = %f\n", x_left, y_top, x_right, y_bottom));	/* for debug */

  if (event->button == 1) {
    pixbuf = gdk_pixbuf_get_from_drawable(pixbuf, pixmap, gdk_rgb_get_colormap(), x_left, y_top, 0, 0, sel_width, sel_height);
    save_spect_dialog(widget, NULL);
  }
}


/* show the frequency in a suitable place (How suitable is stdout?) */
/* for n_zoom>1, accuracy could be better */
static gboolean drawing_area_button_press_event(GtkWidget * widget, GdkEventButton * event)
{
  if (event->button == 1) {
    D(printf("frequency: %.0f Hz\n", da_to_act_y(event->y)));	/* for debug */
  }
  return FALSE; /* event should be propagated further, as it might be handled by other callbacks */
}


void selection_button_press_event(GtkWidget * widget, GdkEventButton * event)
{
  if (event->button == 1) {
    /* store the press coordinates */
    x_down = (gint) event->x;
    y_down = (gint) event->y;

    /* connect the motion signal so we see a rectangle */
    g_signal_connect(G_OBJECT(widget), "motion_notify_event", G_CALLBACK(selection_event), NULL);

    /* connect the release signal to process the selected region */
    g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(release_event), NULL);
  }
}

/* 0.3.5 EG became obsolete
static void freq_ad_changed(GtkWidget * widget, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT(widget);
  pixmap_ofs = adj->value;
}
*/


void stop_start_button_set_label(gchar * label)
{
  /* change label on main window button */
  gtk_label_set_text(GTK_LABEL(GTK_BIN(stop_start_bu)->child), label);
}


/* most of this function was taken from specgrm2, */
/*   Copyright (C) 1995  Philip VanBaren          */
void set_palette(int p_n)
{
  long c, color, _draw_colors = 256;
  unsigned char *p = colortab;

  //p[0] = p[1] = p[2] = 0;
  //p[3] = p[4] = p[5] = 255;
  //p[6] = p[7] = p[8] = 64;

  //p += 9;
  for (c = 0; c < _draw_colors; c++) {
    color = c * 256;
    color /= _draw_colors;
    if (p_n == HSV) {		/* HSV */
      if (color < 64) {
	*(p++) = 0;
	*(p++) = (unsigned char) (color * 4.0);
	*(p++) = 255;
      } else if (color < 128) {
	*(p++) = 0;
	*(p++) = 255;
	*(p++) = (unsigned char) (510.0 - color * 4.0);
      } else if (color < 192) {
	*(p++) = (unsigned char) (color * 4.0 - 510.0);
	*(p++) = 255;
	*(p++) = 0;
      } else {
	*(p++) = 255;
	*(p++) = (unsigned char) (1020.0 - color * 4.0);
	*(p++) = 0;
      }
    } else if (p_n == THRESH) {	/* Thresholded HSV */
      if (color < 16) {
	*(p++) = 0;
	*(p++) = 0;
	*(p++) = 0;
      } else if (color < 64) {
	*(p++) = 0;
	*(p++) = (unsigned char) (color * 4.0);
	*(p++) = 255;
      } else if (color < 128) {
	*(p++) = 0;
	*(p++) = 255;
	*(p++) = (unsigned char) (510.0 - color * 4.0);
      } else if (color < 192) {
	*(p++) = (unsigned char) (color * 4.0 - 510.0);
	*(p++) = 255;
	*(p++) = 0;
      } else {
	*(p++) = 255;
	*(p++) = (unsigned char) (1020.0 - color * 4.0);
	*(p++) = 0;
      }
    } else if (p_n == COOL) {	/* cool */
      *(p++) = (unsigned char) color;
      *(p++) = (unsigned char) (255 - color);
      *(p++) = 255;
    } else if (p_n == HOT) {	/* hot */
      if (color < 96) {
	*(p++) = (unsigned char) (color * 2.66667 + 0.5);
	*(p++) = 0;
	*(p++) = 0;
      } else if (color < 192) {
	*(p++) = 255;
	*(p++) = (unsigned char) (color * 2.66667 - 254);
	*(p++) = 0;
      } else {
	*(p++) = 255;
	*(p++) = 255;
	*(p++) = (unsigned char) (color * 4.0 - 766.0);
      }
    } else if (p_n == BONE) {	/* bone */
      if (color < 96) {
	*(p++) = (unsigned char) (color * 0.88889);
	*(p++) = (unsigned char) (color * 0.88889);
	*(p++) = (unsigned char) (color * 1.20000);
      } else if (color < 192) {
	*(p++) = (unsigned char) (color * 0.88889);
	*(p++) = (unsigned char) (color * 1.20000 - 29);
	*(p++) = (unsigned char) (color * 0.88889 + 29);
      } else {
	*(p++) = (unsigned char) (color * 1.20000 - 60);
	*(p++) = (unsigned char) (color * 0.88889 + 29);
	*(p++) = (unsigned char) (color * 0.88889 + 29);
      }
    } else if (p_n == COPPER) {	/* copper */
      if (color < 208) {
	*(p++) = (unsigned char) (color * 1.23);
	*(p++) = (unsigned char) (color * 0.78);
	*(p++) = (unsigned char) (color * 0.5);
      } else {
	*(p++) = 255;
	*(p++) = (unsigned char) (color * 0.78);
	*(p++) = (unsigned char) (color * 0.5);
      }
    } else if (p_n == OTD) {	/* where did I take this palette ?? */
      if (color < 128) {
	*(p++) = 0;
	*(p++) = (unsigned char) (2.0 * color - 1.0);
	*(p++) = (unsigned char) (2.0 * (127.0 - color) + 1.0);
      } else {
	*(p++) = (unsigned char) (2.0 * (color - 127.0) - 1.0);
	*(p++) = (unsigned char) (2.0 * (255.0 - color) + 1.0);
	*(p++) = 0;
      }
    } else {			/* Black and white */
      *(p++) = (unsigned char) color;
      *(p++) = (unsigned char) color;
      *(p++) = (unsigned char) color;
    }
  }
}


GtkWidget *main_window_init(int nr, float min, float max)
{
  GtkWidget *menu_bar;
  GtkWidget *tmp_hb, *button, *fr_frame, *freq_la, *pwr_frame, *cpu_frame, *peak_frame;

  if (window == NULL) {

    set_palette(opt.palette);

/*
    {
      FILE *fout=fopen("pal", "w");
      int c;

      set_palette(HSV);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(THRESH);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(COOL);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(HOT);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(BW);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(BONE);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(COPPER);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      set_palette(OTD);
      for (c = 0; c < 256; c++) fprintf(fout, "%i %i %i\n", colortab[3 * c], colortab[3 * c + 1], colortab[3 * c + 2]); fprintf(fout, "\n");
      fclose(fout);
    }
*/
    n = nr;
    min_freq = min;
    max_freq = max;
    center_freq = (max_freq + min_freq) / 2.0;

    /* main window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    window_width = l + SPEC_DA_WIDTH;
    window_height = (n < 600 ? n : 600) + 200;
    gtk_window_set_default_size(GTK_WINDOW(window), window_width, window_height);
    gtk_window_set_title(GTK_WINDOW(window), PACKAGE_STRING);

    g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);
    gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
    //gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);

    /* Create a vbox */
    main_vb = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), main_vb);
    gtk_widget_show(main_vb);

    /* Create the menu */
    get_main_menu(window, &menu_bar);
    gtk_box_pack_start(GTK_BOX(main_vb), menu_bar, FALSE, FALSE, 0);
    gtk_widget_show(menu_bar);


    /* create a scrolled window */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    //GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(main_vb), scrolled_window, TRUE, TRUE, 0);

    /* Create a hbox */
    tmp_hb = gtk_hbox_new(FALSE, 0);

    /* istantaneous spectrum drawing area */
    spec_da = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(spec_da), SPEC_DA_WIDTH, pixmap_height);
    g_signal_connect(G_OBJECT(spec_da), "expose_event", G_CALLBACK(spec_da_expose_event), NULL);
    g_signal_connect(G_OBJECT(spec_da), "configure_event", G_CALLBACK(spec_da_configure_event), NULL);
//    g_signal_connect(G_OBJECT(drawing_area), "button_press_event", G_CALLBACK(drawing_area_button_press_event), NULL);
    gtk_widget_set_events(spec_da, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

    gtk_box_pack_start(GTK_BOX(tmp_hb), spec_da, FALSE, FALSE, 1);

    /* spectrogram drawing area */
    drawing_area = gtk_drawing_area_new();
    pixmap_height = n;
    pixmap_width = l;
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), pixmap_width, pixmap_height);
    g_signal_connect(G_OBJECT(drawing_area), "expose_event", G_CALLBACK(drawing_area_expose_event), NULL);
    g_signal_connect(G_OBJECT(drawing_area), "configure_event", G_CALLBACK(drawing_area_configure_event), NULL);
    g_signal_connect(G_OBJECT(drawing_area), "button_press_event", G_CALLBACK(drawing_area_button_press_event), NULL);
    gtk_widget_set_events(drawing_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

    gtk_box_pack_start(GTK_BOX(tmp_hb), drawing_area, TRUE, TRUE, 1);
    gtk_widget_show(drawing_area);

    //gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), spec_hbox);    
    //gtk_widget_show(scrolled_window);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), tmp_hb);
    gtk_widget_show(tmp_hb);

    gtk_widget_show(spec_da);

    gtk_widget_show(scrolled_window);

    /* Create the frequency scrollbar and relative adjustment */

    freq_ad = (GtkAdjustment *) gtk_adjustment_new(0, 0, n, 1, 100, pixmap_height);
/*
    spect_vs = gtk_vscrollbar_new(GTK_ADJUSTMENT(freq_ad));
    gtk_box_pack_start(GTK_BOX(tmp_hb), spect_vs, FALSE, FALSE, 0);
    gtk_widget_show(spect_vs);
    g_signal_connect (GTK_OBJECT (freq_ad), "value_changed", GTK_SIGNAL_FUNC (freq_ad_changed), drawing_area);
*/
    gtk_widget_show(tmp_hb);

    /* Create a hbox */
    tmp_hb = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vb), tmp_hb, FALSE, FALSE, 0);
    //gtk_container_add(GTK_CONTAINER(main_vb), tmp_hb);

    /* Spectrogram info frame */
    fr_frame = gtk_frame_new("Frequency Time (abs/rel) Power");
    gtk_box_pack_start(GTK_BOX(tmp_hb), fr_frame, TRUE, TRUE, 5);
    gtk_widget_show(fr_frame);

    freq_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(fr_frame), freq_la);
    gtk_widget_show(freq_la);

    g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event", G_CALLBACK(drawing_area_motion_event), freq_la);
    g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event", G_CALLBACK(motion_notify_event), freq_la);

    /* Peak level info frame */
    peak_frame = gtk_frame_new("Spectrum peak info");
    gtk_box_pack_start(GTK_BOX(tmp_hb), peak_frame, FALSE, FALSE, 5);
    gtk_widget_show(peak_frame);

    peak_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(peak_frame), peak_la);
    gtk_widget_show(peak_la);

    gtk_widget_show(tmp_hb);

    /* Power frame */
    pwr_frame = gtk_frame_new("Power level");
    gtk_box_pack_start(GTK_BOX(tmp_hb), pwr_frame, FALSE, FALSE, 5);
    gtk_widget_show(pwr_frame);

    pwr_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(pwr_frame), pwr_la);
    gtk_widget_show(pwr_la);

    gtk_widget_show(tmp_hb);

    /* CPU usage frame */
    cpu_frame = gtk_frame_new("CPU usage");
    gtk_box_pack_start(GTK_BOX(tmp_hb), cpu_frame, FALSE, FALSE, 5);
    gtk_widget_show(cpu_frame);

    cpu_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(cpu_frame), cpu_la);
    gtk_widget_show(cpu_la);

    gtk_widget_show(tmp_hb);

    /* hbox for avg info */
    avg_hb = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vb), avg_hb, FALSE, FALSE, 0);
    
    /* Stop/Start button */
    stop_start_bu = gtk_button_new_with_label("Start");
    gtk_box_pack_start(GTK_BOX(main_vb), stop_start_bu, FALSE, FALSE, 10);

    g_signal_connect(G_OBJECT(stop_start_bu), "clicked", G_CALLBACK(toggle_stop_start), NULL);
    gtk_widget_show(stop_start_bu);
    /* ...quit button */
    button = gtk_button_new_with_label("Quit");
    gtk_box_pack_start(GTK_BOX(main_vb), button, FALSE, FALSE, 10);

    g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(window));
    gtk_widget_show(button);

    gtk_widget_show(window);

    //sel_gc = gdk_gc_new(window->window);
    //gdk_gc_set_function (sel_gc, GDK_COPY);

    sel_gc = gdk_gc_new(window->window); 
    gdk_gc_copy(sel_gc, window->style->black_gc); 
    gdk_gc_set_function(sel_gc,GDK_INVERT);

    /* do the cursors */
    cursors[0] = gdk_cursor_new(GDK_CROSSHAIR);
    cursors[1] = gdk_cursor_new(GDK_LEFT_PTR);
    gdk_window_set_cursor(drawing_area->window, cursors[0]);
  } else {
    n = nr;
    min_freq = min;
    max_freq = max;
    center_freq = (max_freq + min_freq) / 2.0;
    /* drawing area */
    //drawing_area = gtk_drawing_area_new();
    pixmap_height = n;
    pixmap_width = l;

    //freq_ad->lower = 0;
    freq_ad->upper = n;
    //freq_ad->pixmap_height = pixmap_height;
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), pixmap_width, pixmap_height);
    //gtk_signal_emit_by_name(GTK_OBJECT(drawing_area), "configure_event");
    /* force widget using the sdjustment to reconfigure */
    gtk_signal_emit_by_name(GTK_OBJECT(freq_ad), "changed");

//    gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), drawing_area->allocation.x, drawing_area->allocation.y);

    //gtk_widget_show(drawing_area);

    //gdk_window_resize(drawing_area->window, pixmap_width, pixmap_height);
    //gtk_widget_queue_resize(GTK_WIDGET(drawing_area));

  }
  /* if needed, insert or remove average info frames */
  if (opt.averaging != NO_AVG)
    insert_avg_frames();
  else
    remove_avg_frames();

  glfer.first_buffer = TRUE;
  return window;
}


static void insert_avg_frames(void) {
 
  if (avgmax_frame == NULL) {
    // maximum above average spectral power
    avgmax_frame = gtk_frame_new("Max-Avg");
    gtk_box_pack_start(GTK_BOX(avg_hb), avgmax_frame, FALSE, FALSE, 5);
    gtk_widget_show(avgmax_frame);
  
    avgmax_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(avgmax_frame), avgmax_la);
    gtk_widget_show(avgmax_la);
  }
  
  if (avgvar_frame == NULL) {
    // variance above average spectral power
    avgvar_frame = gtk_frame_new("Max-Var");
    gtk_box_pack_start(GTK_BOX(avg_hb), avgvar_frame, FALSE, FALSE, 5);
    gtk_widget_show(avgvar_frame);
    
    avgvar_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(avgvar_frame), avgvar_la);
    gtk_widget_show(avgvar_la);
  }
  
  if (avgfill_frame == NULL) {
    // averaging buffer fill ratio
    avgfill_frame = gtk_frame_new("Avg Fill");
    gtk_box_pack_start(GTK_BOX(avg_hb), avgfill_frame, FALSE, FALSE, 5);
    gtk_widget_show(avgfill_frame);
    
    avgfill_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(avgfill_frame), avgfill_la);
    gtk_widget_show(avgfill_la);
  }
  
  if (peakfreq_frame == NULL) {
    // peak power frequency
    peakfreq_frame = gtk_frame_new("Peak F");
    gtk_box_pack_start(GTK_BOX(avg_hb), peakfreq_frame, FALSE, FALSE, 5);
    gtk_widget_show(peakfreq_frame);
    
    peakfreq_la = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(peakfreq_frame), peakfreq_la);
    gtk_widget_show(peakfreq_la);
  }

  gtk_widget_show(avg_hb);
}


static void remove_avg_frames(void) {
  /* remove the avg info frames */

  if (avgmax_frame != NULL) {
    /* make sure the widget pointers are NULL after widget destruction */
    g_signal_connect(G_OBJECT(avgmax_frame), "destroy", G_CALLBACK(gtk_widget_destroyed), &avgmax_frame);
    gtk_widget_destroy(avgmax_frame);
  }

  if (avgvar_frame != NULL) {
    g_signal_connect(G_OBJECT(avgvar_frame), "destroy", G_CALLBACK(gtk_widget_destroyed), &avgvar_frame);
    gtk_widget_destroy(avgvar_frame);
  }

  if (avgfill_frame != NULL) {
    g_signal_connect(G_OBJECT(avgfill_frame), "destroy", G_CALLBACK(gtk_widget_destroyed), &avgfill_frame);
    gtk_widget_destroy(avgfill_frame);
  }

  if (peakfreq_frame != NULL) {
    g_signal_connect(G_OBJECT(peakfreq_frame), "destroy", G_CALLBACK(gtk_widget_destroyed), &peakfreq_frame);
    gtk_widget_destroy(peakfreq_frame);
  }
}


/* draw the data onto the pixmap */
void main_window_draw(float *psdbuf)
{
  GdkRectangle update_rect;
  int i, j, i_off;
  static int x = 0;
  int sx_old = 0, sx;
  float f, f_i, f_off;
  float tmpf = 0.0;
  static float display_max_lvl = 0, display_min_lvl = 0;
  float display_max, display_min;
  float sig_level, thr_level;
  unsigned char v;
  int minbin, maxbin;		
  int peakbin;			
  float binsize;		
  int max0;			// maximum to 0dB indicator

  if (x >= l)
    x = 0;

  /* avoid warning about unused variables */
  i_off = 0;
  f_i = 0.0;
  f_off = 0.0;
  //i_off = opt.data_block_size * (fabs(opt.dfcw_dot_freq - opt.dfcw_dash_freq) / opt.sample_rate);

  thr_level = opt.thr_level / 100.0;	/* normalize threshold level */

  /* clear the spectrum amplitude pixmap */
  gdk_draw_rectangle(spec_pixmap, spec_da->style->fg_gc[GTK_WIDGET_STATE(drawing_area)], TRUE, 0, 0, SPEC_DA_WIDTH, pixmap_height);
#if 0
  /* this generates a linear shape spectrum, useful for testing */
  for (i = 0; i < n; i++)
    psdbuf[i] = i / (n - 1);
#endif

  /* total signal power and mean noise floor power */
  compute_floor(psdbuf, n, &sig_pwr, &floor_pwr, &peak_pwr, &peak_bin); 

  if (opt.autoscale) {		// EG only for (original) autoscale mode
    if (glfer.first_buffer == TRUE) {
      /* when settings are changed need to estimate the PSD taking into account that the "old" samples are all zero and only the "overlapped part" is actually filled with real data */
      if (opt.data_blocks_overlap > 0.0) {
	sig_pwr /= opt.data_blocks_overlap;
	floor_pwr /= opt.data_blocks_overlap;
      }
      display_max_lvl = sig_pwr;
      display_min_lvl = floor_pwr;
      glfer.first_buffer = FALSE;
    } else {
    display_max_lvl = (1.0 - 0.99) * sig_pwr + 0.99 * display_max_lvl;
    display_min_lvl = (1.0 - 0.99) * floor_pwr + 0.99 * display_min_lvl;
    }
  } else {
    display_max_lvl = pow(10.0, opt.max_level_db / 10.0);	
    display_min_lvl = pow(10.0, opt.min_level_db / 10.0);	
    display_min_lvl = (display_max_lvl > display_min_lvl ? display_min_lvl : display_max_lvl / 10.0);	// EG prevent stupid entries
    //display_max_lvl=1e-5; display_min_lvl=1e-10; // 0.3.4 EG experiment use fixed levels
  }

  if ((opt.scale_type == SCALE_LOG) || (opt.scale_type == SCALE_LOG_MAX0)) { // EG added MAX0
    /* convert levels to dB */
    display_max = 10.0 * log10(display_max_lvl);
    display_min = 10.0 * log10(display_min_lvl);
  } else {
    display_max = display_max_lvl;
    display_min = display_min_lvl;
  }

  //fprintf(stderr, "sig = %.3e\tfloor = %.3e\tdisp_max = %.3e\tdisp_min = %.3e\n", sig_pwr, floor_pwr, display_max_lvl, display_min_lvl);
  //fprintf(stderr, "display_max = %e\tdisplay_min = %e\n", display_max, display_min);

  binsize = (float) opt.sample_rate / (float) opt.data_block_size;
  minbin = (int) (opt.min_avgband / binsize);
  maxbin = (int) (opt.max_avgband / binsize);

  if ((opt.scale_type == SCALE_LIN_MAX0) || (opt.scale_type == SCALE_LOG_MAX0))	// EG set the max to 0dB indicator for sumavg and sumextreme
    max0 = 1;
  else
    max0 = 0;

  switch (opt.averaging) {	
    case NO_AVG:
      glfer.avgmax = 1e-15;
      glfer.avgvar = 1e-15;
      glfer.peakfreq = 0.0;
      glfer.peakval = 0.0;
      break;
    case AVG_SUMAVG:
      glfer.avgmax = update_avg_sumavg(&avgdata, n, psdbuf, max0, minbin, maxbin, &peakbin, &glfer.avgvar);
      if (glfer.avgfill < opt.avgsamples)
	glfer.avgfill++;
      glfer.peakfreq = ((float) peakbin) * binsize;
      glfer.peakval = 0.0;
      break;
    case AVG_PLAIN:
      glfer.avgmax = update_avg_plain(&avgdata, n, psdbuf, minbin, maxbin, &peakbin);
      glfer.avgvar = 1e-15;
      if (glfer.avgfill < opt.avgsamples)
	glfer.avgfill++;
      glfer.peakfreq = ((float) peakbin) * binsize;
      glfer.peakval = avgdata.avg[peakbin];
      break;
    case AVG_SUMEXTREME:
      glfer.avgmax = update_avg_sumextreme(&avgdata, n, psdbuf, max0, minbin, maxbin, &peakbin);
      glfer.avgvar = 1e-15;
      if (glfer.avgfill < opt.avgsamples)
	glfer.avgfill++;
      glfer.peakfreq = ((float) peakbin) * binsize;
      glfer.peakval = 0.0;
      break;
  }

  /* i is the pixmap y coordinate */
  for (i = 0; i < n; i++) {
    /* max. value for psdbuf[i] should be 1, for sine wave input, due to */
    /* the normalization in fft.c. min. value is 0 */
    /* input relative value in dB below maximum */

    if ((opt.scale_type == SCALE_LOG) || (opt.scale_type == SCALE_LOG_MAX0)) { // EG added MAX0
      if (opt.averaging != NO_AVG) {
	sig_level = levbuf[x * pixmap_height + i] = 10.0 * log10(avgdata.avg[n - i - 1]);
      } else {
	sig_level = levbuf[x * pixmap_height + i] = 10.0 * log10(psdbuf[n - i - 1]);
      }
    } else {
      if (opt.averaging != NO_AVG)
	sig_level = avgdata.avg[n - i - 1];
      else
	sig_level = psdbuf[n - i - 1];
      /* levels buffer is always LOG */
      levbuf[x * pixmap_height + i] = 10.0 * log10(sig_level);
    }

    f = 255 * ((sig_level - display_min) / (display_max - display_min));

    /* be careful; v is unsigned char and will rollover when f<0 ! */

    /*
       f_i = psdbuf[n - i - 1];
       if (i < i_off) {
       f_off = psdbuf[n - i - 1];
       v = f = 256 + 25 * log10(f_i);
       } else {
       f_off = psdbuf[n - i - 1 - i_off];
       v = f = 256 + 25 * log10(f_i - f_off);
       }
     */

    if (f < 255.0 * thr_level) {	/* below threshold */
      v = 0;
      D(fprintf(stderr, "below threshold (%.2f%%)!\n", thr_level * 100.0));
    } else if (f > 255) {
      v = 255;
      D(fprintf(stderr, "over 255!\n"));
    } else {
      v = (f - 255.0 * thr_level) / (1.0 - thr_level);
    }


    for (j = 0; j < n_zoom; j++) {
      rgbbuf[3 * (n_zoom * i + j)] = colortab[3 * v];
      rgbbuf[3 * (n_zoom * i + j) + 1] = colortab[3 * v + 1];
      rgbbuf[3 * (n_zoom * i + j) + 2] = colortab[3 * v + 2];
    }

    if (i == 0) {
      sx_old = SPEC_DA_WIDTH * f / 255.0;
    } else {
      sx = SPEC_DA_WIDTH * f / 255.0;

      /* draw spectrum amplitude pixmap */
      gdk_draw_line(spec_pixmap, spec_da->style->white_gc, sx_old, i - 1, sx, i);
      sx_old = sx;
    }
  }

  /* draw threshold level line in current spectrum window */
  gdk_draw_line(spec_pixmap, spec_da->style->white_gc, SPEC_DA_WIDTH * thr_level, 0, SPEC_DA_WIDTH * thr_level, n_zoom * n);
  /* draw an entire line of the spectrogram */
  x_cur = x;
  gdk_draw_rgb_image(pixmap, drawing_area->style->fg_gc[GTK_STATE_NORMAL], x, 0, 1, n_zoom * n, GDK_RGB_DITHER_NONE, rgbbuf, 3);
  /* draw the boundary between old and new spectrogram */
  gdk_draw_line(pixmap, drawing_area->style->fg_gc[GTK_STATE_NORMAL], x + 1, 0, x + 2, n_zoom * n);
  update_rect.x = x;
  update_rect.width = 2;
  update_rect.y = 0;
  update_rect.height = n_zoom * n;

  gtk_widget_draw(drawing_area, &update_rect);

  update_rect.x = 0;
  update_rect.width = SPEC_DA_WIDTH;
  update_rect.y = 0;
  update_rect.height = n_zoom * n;

  gtk_widget_draw(spec_da, &update_rect);
  update_cpu_usage(cpu_la);
  update_pwr(pwr_la);
  update_peak(peak_la);

  if (opt.averaging != NO_AVG) {
    update_avgmax(avgmax_la);
    update_avgvar(avgvar_la);
    update_avgfill(avgfill_la);
    update_peakfreq(peakfreq_la);
  }
  
  x++;
}


void main_window_close()
{
  gdk_cursor_unref(cursors[0]);
  gdk_cursor_unref(cursors[1]);
  gdk_gc_unref(sel_gc);
  FREE_MAYBE(rgbbuf);
  gtk_main_quit();
}


static void select_mode(GtkWidget * widget, gpointer data)
{
  int mode_sel = (int) data;
/*
  GtkCheckMenuItem *item = NULL;

  item = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(GTK_ITEM_FACTORY(widget), "/Mode/FFT"));
  g_print("FFT active : %i\n", item->active);

  item = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(GTK_ITEM_FACTORY(widget), "/Mode/MTM"));
  g_print("MTM active : %i\n", item->active);

 item = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(GTK_ITEM_FACTORY(widget), "/Mode/HPARMA"));
  g_print("HPARMA active : %i\n", item->active);
*/
#ifdef DEBUG
  switch (mode_sel) {
  case MODE_FFT:
    g_print("select_mode: MODE_FFT\n");
    break;
  case MODE_MTM:
    g_print("select_mode: MODE_MTM\n");
    break;
  case MODE_HPARMA:
    g_print("select_mode: MODE_HPARMA\n");
    break;
  case MODE_LMP:
    g_print("select_mode: MODE_LMP\n");
    break;
  default:
    g_print("select_mode: MODE_UNKNOWN\n");
  }
#endif
  change_params(mode_sel);
}


void save_full_pixmap(GtkWidget * widget, gpointer data)
{
  pixbuf = gdk_pixbuf_get_from_drawable(NULL, pixmap, gdk_rgb_get_colormap(), 0, 0, 0, 0, pixmap_width, pixmap_height);

  save_spect_dialog(widget, NULL);
}


void save_pixmap_region(GtkWidget * widget, gpointer data)
{
  /* change to the cursor used for selection */
  gdk_window_set_cursor(drawing_area->window, cursors[1]);
  /* connect the press signal to start selection */
  g_signal_connect(G_OBJECT(drawing_area), "button_press_event", G_CALLBACK(selection_button_press_event), NULL);
}
