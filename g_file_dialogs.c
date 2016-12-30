/* g_file_dialogs.c

 * Copyright (C) 2009 Claudio Girardi
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

#include <gtk/gtk.h>
#include <string.h>
#include "glfer.h"
#include "g_file_dialogs.h"

extern glfer_t glfer;
extern GdkPixbuf *pixbuf;


static void spectrum_file_save(gchar *filename);


void open_file_dialog(GtkWidget * widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;

  dialog = gtk_file_chooser_dialog_new ("Open File", NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
  
  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "Wav");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_filter_add_pattern (filter, "*.[Ww][Aa][Vv]");

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "All files");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_filter_add_pattern (filter, "*");

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename;
    
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    stop_reading_audio();
    init_audio(filename);
    start_reading_audio();
    glfer.first_buffer = TRUE; /* need to reinitialize display autoscale */
    g_free(filename);
  }
  
  gtk_widget_destroy (dialog);
}


void save_spect_dialog(GtkWidget * widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gchar *filter_name, *filename_ext = NULL, *tmps;
  gboolean file_format_ok;

  dialog = gtk_file_chooser_dialog_new ("Save File", NULL, 
					GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER(dialog), TRUE);

  /*
  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "Supported formats");
  gtk_file_filter_add_pixbuf_formats(filter);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
  */

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "JPEG");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_filter_add_pattern (filter, "*.[Jj][Pp][Gg]");

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "PNG");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_filter_add_pattern (filter, "*.[Pp][Nn][Gg]");

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "BMP");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_filter_add_pattern (filter, "*.[Bb][Mm][Pp]");

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name (filter, "All files");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  gtk_file_filter_add_pattern (filter, "*");

  //if (user_edited_a_new_document) {
  // gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), default_folder_for_saving);
  //gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "glfer.jpg");
  //} else 
  //  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename_for_existing_document);
  
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename;
    
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    /* get current selected filter */
    filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
    filter_name = gtk_file_filter_get_name(filter);
    
    file_format_ok = FALSE;
    /* check if selected filensme has an extension */
    filename_ext = strrchr(filename, '.');
    if (filename_ext) {
      filename_ext++; /* point to extension following the dot */
      if (g_str_has_suffix(filename, "jpg") || g_str_has_suffix(filename, "JPG") ||
	  g_str_has_suffix(filename, "png") || g_str_has_suffix(filename, "PNG") ||
	  g_str_has_suffix(filename, "bmp") || g_str_has_suffix(filename, "BMP")) {
	/* user has specified a known extension, so file format to use is known */
	/* filename already has the correct extension */
	file_format_ok = TRUE;
      }
    } else { /* user did not specify file estension, use the one from the filter */
      if (!strcmp(filter_name, "JPEG")) {
	filename_ext = strdup(".jpg");
      } else if (!strcmp(filter_name, "PNG")) {
	filename_ext = strdup(".png");
      } else if(!strcmp(filter_name, "BMP")) {
	filename_ext = strdup(".bmp");
      }
      
      if (strcmp(filter_name, "All files")) {/* if a filter other than "All files" is selected */	
	/* add suffix to filename */
   	tmps = g_strconcat(filename, filename_ext, NULL);
	g_free(filename); /* no longer needed */
	g_free(filename_ext); /* no longer needed */
	filename = tmps;
	file_format_ok = TRUE;
      } /* else file_format_ok = FALSE */
    }

    if (file_format_ok == TRUE) {
      g_printf("filename = %s\n", filename);
      spectrum_file_save(filename);
    }
    g_free (filename);
  }
  
  gtk_widget_destroy (dialog);
}


static void spectrum_file_save(gchar *filename) 
{
  GError *error = NULL;
  gboolean save_ok = 0;

  if (g_str_has_suffix(filename, ".jpg") || g_str_has_suffix(filename, ".JPG"))
    save_ok = gdk_pixbuf_save (pixbuf, filename, "jpeg", &error, "quality", "100", NULL);
  else
    if (g_str_has_suffix(filename, ".png") || g_str_has_suffix(filename, ".PNG"))
      save_ok = gdk_pixbuf_save (pixbuf, filename, "png", &error, NULL);
    else
      if (g_str_has_suffix(filename, ".bmp") || g_str_has_suffix(filename, ".BMP"))
	save_ok = gdk_pixbuf_save (pixbuf, filename, "bmp", &error, NULL);

  if (!save_ok) {
    /* something went wrong, print error message and free memory */
    g_warning("%s\n", error->message);
    g_error_free(error);
    gdk_pixbuf_unref(pixbuf);
    //g_string_free(filename, TRUE);
    //return FALSE;
  }
  /* everything went OK, free memory */
  //g_string_free(filename, TRUE);
  gdk_pixbuf_unref(pixbuf);
  //return TRUE;
}
