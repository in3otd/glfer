/* g_txmsg.c

 * Copyright (C) 2001-2003 Claudio Girardi
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
#include <math.h>
#include <time.h>
#include <ctype.h>		/* for toupper() and islower() */
#include <gtk/gtk.h>
#include "g_txmsg.h"
#include "glfer.h"
#include "util.h"
#include "qrs.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif


extern opt_t opt;
extern glfer_t glfer;

static GtkWidget *window = NULL;
static GtkWidget *entry = NULL, *time_la = NULL, *time_left_la = NULL, *time_left_desc_la = NULL;
static GtkWidget *start_button, *stop_button;
static guint function_tag = 0;
static float msg_time_msec;
time_t stop_time;

/* function prototypes */
static gint countdown_timer(gpointer data);

static void timestr(float time_sec, gchar ** str)
{
  int time_h, time_m, time_s;

  time_h = time_sec / 3600;
  time_sec -= time_h * 3600;
  time_m = time_sec / 60;
  time_sec -= time_m * 60;
  time_s = time_sec;
  *str = g_strdup_printf("%ih%im%is", time_h, time_m, time_s);
}


static void enable_beacon_mode(GtkWidget * widget, gpointer data)
{
  opt.beacon_mode = GTK_TOGGLE_BUTTON(widget)->active;
}


static gint countdown_timer(gpointer data)
{
  int msg_char_index;
  gchar *tmp_str;
  time_t time_left, curr_time;

  time(&curr_time);
  time_left = stop_time - curr_time;

  if (time_left >= 0) {
    /* convert to hms string */
    timestr(time_left, &tmp_str);
    gtk_label_set_text(GTK_LABEL(time_left_la), tmp_str);
    g_free(tmp_str);

    msg_char_index = get_qrss_char_index();
    gtk_editable_select_region(GTK_EDITABLE(entry), msg_char_index, msg_char_index + 1);
    /* don't stop calling this timeout function */
    return TRUE;
  } else if (opt.beacon_mode) { /* beacon mode */
    msg_char_index = get_qrss_char_index();
    
    /* get current time */
    time(&stop_time);
    /* compute stop time */
    stop_time += (msg_time_msec / 1000) + opt.beacon_pause;
    return TRUE;
    /* don't stop calling this timeout function */
  } else { /* end of message */
    /* hide time left indications */
    gtk_widget_hide(time_left_desc_la);
    gtk_widget_hide(time_left_la);
    /* message transmission ended, enable editing of entry text */
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    /* enable start and disable stop buttons */
    gtk_widget_set_sensitive(start_button, TRUE);
    gtk_widget_set_sensitive(stop_button, FALSE);
    /* stop calling this timeout function */
    function_tag = 0;
    return FALSE;
  }
}


static void start_message(GtkWidget * widget, gpointer * data)
{
  guint upd_time;

  /* disable editing of text to be transmitted */
  gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
  /* disable start and enable stop buttons */
  gtk_widget_set_sensitive(start_button, FALSE);
  gtk_widget_set_sensitive(stop_button, TRUE);
  /* select message first character */
  gtk_editable_select_region(GTK_EDITABLE(entry), 0, 1);
  /* get message time duration (might have changed due to mode change) */
  msg_time_msec = string_duration(gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1));
  gtk_widget_show(time_left_desc_la);
  gtk_widget_show(time_left_la);
  /* get current time */
  time(&stop_time);
  /* compute stop time */
  stop_time += (msg_time_msec / 1000) + 0.5;
  if (opt.beacon_mode == TRUE)
    stop_time += opt.beacon_pause;
  /* add countdown timer function to be called every second or every dot duration, whichever is less */
  upd_time = (opt.dot_time < 1000 ? opt.dot_time : 1000);
  function_tag = gtk_timeout_add(upd_time, countdown_timer, NULL);
  D(g_print("%f\n", string_duration(gtk_editable_get_chars(GTK_EDITABLE(data), 0, -1))));	/* for debug */
  /* send message */
  send_string(gtk_editable_get_chars(GTK_EDITABLE(data), 0, -1));
}


static void stop_message(GtkWidget * widget, gpointer * data)
{
  /* stop transmissions */
  stop_tx();
  /* hide time left indications */
  gtk_widget_hide(time_left_desc_la);
  gtk_widget_hide(time_left_la);
  /* enable editing of entry text */
  gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
  /* enable start and disable stop buttons */
  gtk_widget_set_sensitive(start_button, TRUE);
  gtk_widget_set_sensitive(stop_button, FALSE);
  /* remove countdown timer */
  if (function_tag != 0)
    gtk_timeout_remove(function_tag);
}


static void filter_entry_text(GtkEntry * entry, const gchar * text, gint length, gint * position, gpointer data)
{
  GtkEditable *editable = GTK_EDITABLE(entry);
  int i, count = 0;
  gchar *result = g_new(gchar, length);

  /* analyze char by char */
  for (i = 0; i < length; i++) {
    /* check if char is allowed in CW transmission */
    if (!cw_char_allowed(text[i]) == TRUE)	/* if not, skip to next */
      continue;
    /* force uppercase */
    result[count++] = islower(text[i]) ? toupper(text[i]) : text[i];
  }

  if (count > 0) {
    gtk_signal_handler_block_by_func(GTK_OBJECT(editable), G_CALLBACK(filter_entry_text), data);
    gtk_editable_insert_text(editable, result, count, position);
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(editable), G_CALLBACK(filter_entry_text), data);
  }
  gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");

  g_free(result);
}


static void entry_changed(GtkWidget * widget, gpointer * data)
{
  gchar *tmp_str;
  float tmp_time_msec;

  /* get message time duration */
  tmp_time_msec = string_duration(gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1));
  /* round to the nearest second */
   tmp_time_msec += 500.0;
   if (opt.beacon_mode == TRUE)
     tmp_time_msec += opt.beacon_pause * 1000.0;
  /* convert to hms string */
  timestr(tmp_time_msec / 1000, &tmp_str);
  gtk_label_set_text(GTK_LABEL(time_la), tmp_str);
  gtk_label_set_text(GTK_LABEL(time_left_la), tmp_str);
  g_free(tmp_str);
}


void update_time_indications(void)
{
  if (window)
    entry_changed(entry, NULL);
}


void create_txmsg_window(void)
{
  GtkWidget *vbox, *temp_hb, *temp_la, *be_bu, *cancel_button;

  if (!window) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_widget_destroyed), &window);

    gtk_window_set_title(GTK_WINDOW(window), "TX Message");
    gtk_container_border_width(GTK_CONTAINER(window), 0);

    /* Create a vbox */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);

    /* hbox for the text entry */
    temp_hb = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), temp_hb, FALSE, FALSE, 5);
    /* create the text entry */
    entry = gtk_entry_new();
    gtk_tooltips_set_tip(glfer.tt, entry, "Text to be transmitted", "qqq");
    /* just make the width greater than the default */
    gtk_widget_set_usize(entry, 200, -1);
    gtk_box_pack_end(GTK_BOX(temp_hb), entry, TRUE, TRUE, 5);
    /* filter the input in the entry (from the GTK+ FAQ) */
    g_signal_connect(G_OBJECT(entry), "insert_text", G_CALLBACK(filter_entry_text), NULL);
    /* update timers with message */
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(entry_changed), NULL);
    /* pressing ENTER is the same as clicking on the Start button */
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(start_message), GTK_OBJECT(entry));
    gtk_widget_show(entry);
    /* entry description */
    temp_la = gtk_label_new("Text:");
    gtk_box_pack_end(GTK_BOX(temp_hb), temp_la, FALSE, FALSE, 5);
    gtk_widget_show(temp_la);
    gtk_widget_show(temp_hb);

    /* hbox for the text duration labels */
    temp_hb = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), temp_hb, FALSE, FALSE, 5);
    /* description label */
    temp_la = gtk_label_new("Time:");
    gtk_box_pack_start(GTK_BOX(temp_hb), temp_la, FALSE, FALSE, 5);
    gtk_widget_show(temp_la);
    /* message duration time */
    time_la = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(temp_hb), time_la, FALSE, FALSE, 5);
    gtk_widget_show(time_la);
    /* time left to message end */
    time_left_la = gtk_label_new(NULL);
    gtk_box_pack_end(GTK_BOX(temp_hb), time_left_la, FALSE, FALSE, 5);
    //gtk_widget_show(time_left_la);
    /* description label */
    time_left_desc_la = gtk_label_new("Time left:");
    gtk_box_pack_end(GTK_BOX(temp_hb), time_left_desc_la, FALSE, FALSE, 5);
    //gtk_widget_show(time_left_desc_la);
    gtk_widget_show(temp_hb);

    /* default message */
    gtk_entry_set_text(GTK_ENTRY(entry), "cq");

    /* beacon mode enable check button */
    be_bu = gtk_check_button_new_with_label("Beacon mode");
    gtk_tooltips_set_tip(glfer.tt, be_bu, "Enable/disable beacon mode (repeated message)", NULL);
    gtk_box_pack_start(GTK_BOX(vbox), be_bu, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(be_bu), "clicked", G_CALLBACK(enable_beacon_mode), NULL);
    /* if beacon mode  enabled activate the toggle button */
    if (opt.beacon_mode == TRUE) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be_bu), TRUE);
    } else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be_bu), FALSE);
    }
    gtk_widget_show(be_bu);

    /* Create the buttons area */
    temp_hb = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), temp_hb, TRUE, TRUE, 5);

    /* cancel button */
    cancel_button = gtk_button_new_with_label("Cancel");
    gtk_tooltips_set_tip(glfer.tt, cancel_button, "Close window", NULL);
    gtk_box_pack_end(GTK_BOX(temp_hb), cancel_button, TRUE, TRUE, 0);

    /* pressing Cancel is the same as pressing Stop end closing the window */
    g_signal_connect(G_OBJECT(cancel_button), "clicked", G_CALLBACK(stop_message), GTK_OBJECT(entry));
    g_signal_connect_swapped(G_OBJECT(cancel_button), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(window));
    gtk_widget_show(cancel_button);

    /* transmission start button */
    start_button = gtk_button_new_with_label("Start");
    gtk_tooltips_set_tip(glfer.tt, start_button, "Start message transmission", NULL);
    gtk_box_pack_start(GTK_BOX(temp_hb), start_button, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(start_button), "clicked", G_CALLBACK(start_message), GTK_OBJECT(entry));
    gtk_widget_show(start_button);

    /* transmission stop button */
    stop_button = gtk_button_new_with_label("Stop");
    gtk_tooltips_set_tip(glfer.tt, stop_button, "Stop message transmission", NULL);
    gtk_box_pack_start(GTK_BOX(temp_hb), stop_button, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(stop_message), GTK_OBJECT(entry));
    gtk_widget_show(stop_button);

    gtk_widget_show(temp_hb);

  }
  if (!GTK_WIDGET_VISIBLE(window)) {
    gtk_widget_show(window);
  } else {
    gtk_widget_destroy(window);
  }

}
