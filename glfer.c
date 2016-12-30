/* glfer.c

 * Copyright (C) 2001-2009 Claudio Girardi
 *           (C) 2006      Edouard Griffiths for modifications implementing
 *                         averaging mode
 *
 * Part of this file was inspired by xspectrum, Copyright (C) 2000 Vincent Arkesteijn
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
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <gtk/gtk.h>

#include "source.h"
#include "audio.h"
#include "fft.h"
#include "avg.h"
#include "mtm.h"
#include "hparma.h"
#include "lmp.h"
#include "g_main.h"
#include "qrs.h"
#include "util.h"
#include "rcfile.h"
#include "glfer.h"
#include "wav_fmt.h"
/* #include "jason.h" */

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif

opt_t opt;
glfer_t glfer;
fft_params_t fft_gram_par;
mtm_params_t mtm_gram_par;
hparma_params_t hparma_gram_par;
lmp_params_t lmp_gram_par;
avg_data_t avgdata;

static int psd_n;
static char *fname = NULL;
static float *psdbuf = NULL;
static int rc_file_read_ok;


/* function prototypes */
void show_message(gchar * fmt, ...);
static void destroy(GtkWidget * widget, gpointer data);
static void help(char *name);
static void usage(char *name);
static void version();
static void parse_args(int argc, char *argv[], char **device, int *sample_rate, int *n);


/* Function to open a dialog box displaying the message provided. */
/* ...almost entirely from the GTK Reference Manual... */
void show_message(gchar * fmt, ...)
{
  va_list args;
  gchar *msg_text;
  GtkWidget *dialog, *label, *ok_button;

  /* create a new dialog */
  dialog = gtk_dialog_new();
  /* realize the dialog */
  gtk_widget_realize(dialog);
  /* allow only move and close (no resize) */
  gdk_window_set_functions(dialog->window, GDK_FUNC_MOVE | GDK_FUNC_CLOSE);
  /* show border and title only */
  gdk_window_set_decorations(dialog->window, GDK_DECOR_BORDER | GDK_DECOR_TITLE);
  /* set border width */
  gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);
  /* set dialog title */
  gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
  gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
  /* make the dialog modal */
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

  /* initialise args */
  va_start(args, fmt);
  /* format the whole message into msg_text */
  msg_text = g_strdup_vprintf(fmt, args);
  /* create a label */
  label = gtk_label_new(msg_text);
  /* free unused variable */
  g_free(msg_text);
  gtk_misc_set_padding(GTK_MISC(label), 20, 20);
  /* FIXME: anyone knows how to make line wrapping work ? */
  //gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  //gtk_widget_set_usize (label, 160, 120);

  /* gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL); */
  ok_button = gtk_button_new_with_label("Ok");
  GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);

  /* Ensure that the dialog box is destroyed when the user clicks ok. */
  g_signal_connect_swapped(G_OBJECT(ok_button), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(dialog));
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), ok_button);
  /* make OK the default button */
  gtk_widget_grab_default(ok_button);

  /* Add the label, and show everything we've added to the dialog. */
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
  gtk_widget_show_all(dialog);
  /* done with variable arguments */
  va_end(args);
}


static void destroy(GtkWidget * widget, gpointer data)
{
  FREE_MAYBE(psdbuf);

  FREE_MAYBE(opt.ctrl_device);

  close_serial_port();
  close_ksound();
  audio_close();
  fft_close(&fft_gram_par);
  main_window_close();
  rc_file_done();
}


static void help(char *name)
{
  fprintf(stderr, "%s, by IN3OTD, Version " PACKAGE_VERSION " (" RELDATE ")\n" 
	  "Usage: %s [options]\n" 
	  "Options:\n"
          "\n"
          "  -d, --device FILE       use FILE as audio input device instead of /dev/dsp.\n" 
	  "  -f, --file FILENAME     take audio input from FILENAME (WAV format).\n" 
	  "  -s, --sample_rate RATE  set audio sample rate to RATE [Hz].\n" 
	  "  -n N                    set number of points fot the FFT to N.\n" 
	  "  -h, --help              print this help.\n" 
          "  -v, --version           display the version of glfer and exit.\n", name, name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Mail bug reports and suggestions to <" PACKAGE_BUGREPORT ">\n");
  fprintf(stderr, "\n");
  exit(1);
}


static void usage(char *name)
{
  char s[] = "Usage: %s [options]\n" "Try `%s --help' for more informations\n";

  fprintf(stderr, s, name, name);
}


static void version()
{
  fprintf(stderr,
	  /* State the canonical name for the program */
	  "glfer " PACKAGE_VERSION " (" RELDATE ")\n" 
	  "Copyright (C) 2009 Claudio Girardi <in3otd@qsl.net>\n" 
	  "\n"
	  "This program is distributed in the hope that it will be useful,\n" 
	  "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" 
	  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" 
	  "GNU General Public License for more details.\n" "\n");
}


static void parse_args(int argc, char *argv[], char **device, int *sample_rate, int *n)
{
  int c;
  static char shortopts[] = "d:f:n:s:hv";
  static struct option longopts[] = {
    {"device", 1, NULL, 'd'},
    {"file", 1, NULL, 'f'},
    {"sample_rate", 1, NULL, 's'},
    {"help", 0, NULL, 'h'},
    {"version", 0, NULL, 'v'},
    {"", 0, NULL, 0}
  };
  int option_index = 0;

  while ((c = getopt_long(argc, argv, shortopts, longopts, &option_index)) != -1)
    switch (c) {
    case ('d'):
      *device = strdup(optarg);
      break;
    case ('f'):
      fname = strdup(optarg);
      break;
    case ('n'):
      *n = atoi(optarg);
      break;
    case ('s'):
      opt.sample_rate = atoi(optarg);
      break;
    case ('h'):
      help(argv[0]);
      exit(0);
      break;
    case ('v'):
      version();
      exit(0);
      break;
    case ('?'):
      usage(argv[0]);
      exit(-1);
      break;
    }
}

int main(int argc, char *argv[])
{
  GtkWidget *window;
  int dev_status = 0;

  opt.data_block_size = 1024;
  opt.data_blocks_overlap = 0.0;
  opt.limiter_a = 0.0; /* with 0.0 we have the classical periodogram, otherwise 0.001 ia a good starting value */
  opt.enable_limiter = 0;

  /* MTM parameters */
  opt.mtm_w = 4.0; /* this is actually N * W, according to Thompson notation */
  opt.mtm_k = (2 * opt.mtm_w) - 1;	/* number of windows */

  /* HP ARMA parameters */
  opt.hparma_p_e = 16;
  opt.hparma_t = 96;

  /* LMP parameters */
  opt.lmp_av = 4;

  opt.sample_rate = 8000;
  opt.dot_time = 500.0;
  opt.ptt_delay = 100.0;
  opt.tx_mode = QRSS;
  opt.dfcw_gap_time = 100;
  opt.dash_dot_ratio = 3.0;
  opt.sidetone_freq = 1000.0;
  opt.dfcw_dot_freq = 800.0;
  opt.dfcw_dash_freq = 810.0;
  opt.beacon_mode = FALSE;
  opt.beacon_pause = 5;
  opt.beacon_tx_pause = TRUE;
  opt.sidetone = FALSE;
  opt.mode = MODE_FFT;
  opt.scale_type = SCALE_LOG;
  opt.ctrl_device = malloc(6 * sizeof(char));
  strcpy(opt.ctrl_device, "ttyS1");
  opt.audio_device = malloc(9 * sizeof(char));
  strcpy(opt.audio_device, "/dev/dsp");
  opt.window_type = 7;
  opt.thr_level = 0.0;
  opt.autoscale = 1;  /* autoscale as default, otherwise may be confusing for a new user */
  opt.max_level_db = -3.0;        
  opt.min_level_db = -23.0;       
  opt.min_avgband = 400.0;        
  opt.max_avgband = 1200.0;       

  /* well, this should be more elegant... */
  if (parse_rcfile() == 0)
    rc_file_read_ok = TRUE;
  else
    rc_file_read_ok = FALSE;


  /* ...there's no internationalization support at present, so suppress unnecessary warnings */
  gtk_disable_setlocale ();

  gtk_init(&argc, &argv);

  parse_args(argc, argv, &opt.audio_device, &opt.sample_rate, &opt.data_block_size);

  if (opt.avgsamples <= 0)
    opt.avgsamples = 4;
  //init_avg(&avgdata); // EG - What's the purpose ? CG
  //delete_avg(&avgdata);
  //alloc_avg(&avgdata, opt.data_block_size, opt.avgsamples);    
  
  
  glfer.avgfill = 0.0;

  glfer.input_source = SOURCE_NONE; /* may be modified by init_audio() below */

  /* the following code sequence for setting the visual and the colormap */
  /* is from the GDK Reference Manual */
  gdk_rgb_init();
  gtk_widget_set_default_colormap(gdk_rgb_get_colormap());
  gtk_widget_set_default_visual(gdk_rgb_get_visual());

  /* whole application tooltips */
  glfer.tt = gtk_tooltips_new();

  /* enable sound, if desired */
  if (opt.sidetone == TRUE)
    open_ksound();

  if (rc_file_read_ok) {
    /* rc file read, open specified control device */
    if (opt.device_type == DEV_SERIAL) {
      dev_status = open_serial_port(opt.ctrl_device);
    } else if (opt.device_type == DEV_PARALLEL) {
      dev_status = open_parport(opt.ctrl_device);
    } else {
      g_assert_not_reached();
    }
    /* is everything up and running ? */
    glfer.init_done = (dev_status == -1 ? FALSE : TRUE);
  } else {
    glfer.init_done = FALSE;
  }

  /* set current mode FIXME */
  glfer.current_mode = MODE_NONE;
  init_audio(fname);
  change_params(opt.mode);

/*
  //jason_init(opt.data_block_size); FIXME (calls fft.init)
*/

  psd_n = opt.data_block_size / 2 + 1;

  window = main_window_init(psd_n, 0, (float) opt.sample_rate / 2);

  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);

  if (glfer.init_done == FALSE) {
    /* there is no valid TX control port */
    if (dev_status == 0) {
      /* no TX control port was defined in the preference file */
      show_message("No control port is defined for the transmitter!\n"
		   "\n"
		   "Please go to the Settings/Port menu and select a control\n"
		   "device to enable transmission functions.");
    } else {
      /* dev_status = -1 */
      /* we tried to open a TX control port but without success */
      /* show a warning message */
      show_message("Open %s failed !\n" 
		   "\n"
		   "Please go to the Settings/Port menu and select a control\n" 
		   "device to enable transmission functions.", opt.ctrl_device);
    }
  }

  /* gtk_idle_add(timefn, NULL); */

  gtk_main();

  return (0);
}
