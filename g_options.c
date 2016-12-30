/* g_options.c
 * 
 * Copyright (C) 2001-2009 Claudio Girardi
 *           (C) 2006      Edouard Griffiths for modifications implementing
 *                         averaging mode
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>
#include "g_options.h"
#include "g_txmsg.h"
#include "util.h"
#include "fft.h"
#include "avg.h"
#include "rcfile.h"
#include "qrs.h"
#include "glfer.h"
#include "source.h"
#include "g_main.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif


extern opt_t opt;
extern glfer_t glfer;
extern avg_data_t avgdata;

extern fft_window_t fft_windows[];
extern int num_fft_windows;

static GtkWidget *spec_window = NULL, *qrss_window = NULL, *tx_test_window = NULL, *port_window = NULL, *audio_window = NULL;
static GtkWidget *sn_bu, *be_keyup_bu, *be_keydown_bu, *qrss_bu, *dfcw_bu, *serial_bu, *parallel_bu, *hendrix_bu, *autoscale_bu;
static GtkWidget *offset_en, *data_block_size_en, *data_blocks_overlap_en, *display_update_en, *limiter_a_en, *mtm_w_en, *mtm_k_en, *hparma_t_en, *hparma_p_e_en, *lmp_av_en, *sample_rate_en, *dot_time_en, *control_port_en, *wpm_en, *lin_scale_bu, *log_scale_bu, *ptt_delay_en, *sidetone_fr_en, *dfcw_dot_fr_en, *dfcw_dash_fr_en, *be_pause_en, *level_en, *lin_scale_bu, *lin_scale_max0_bu, *log_scale_bu, *log_scale_max0_bu, *avg_sumavg_bu, *avg_plain_bu, *avg_sumextreme_bu, *noavg_bu, *max_level_en, *min_level_en, *avgsamples_en, *minavgband_en, *maxavgband_en, *audio_device_en, *avgtime_la, *avgintdb_la, *bin_size_la;  ;


typedef struct _palette_t palette_t;

struct _palette_t
{
  gchar *name;
  gint type;
};

static palette_t palettes[] = {
  {"/HSV", HSV},
  {"/Thresholded HSV", THRESH},
  {"/Cool", COOL},
  {"/Hot", HOT},
  {"/Black and White", BW},
  {"/Bone", BONE},
  {"/Copper", COPPER},
  {"/IN3OTD", OTD}
};

static gint num_palettes = sizeof(palettes) / sizeof(palettes[0]);

static void dot_time_en_changed(GtkWidget * widget, gpointer * data);
static void wpm_en_changed(GtkWidget * widget, gpointer * data);
static void data_blocks_overlap_en_changed(GtkWidget * widget, gpointer * data);
static void data_block_size_en_changed(GtkWidget * widget, gpointer * data);
static void display_update_en_changed(GtkWidget * widget, gpointer * data);
static void autoscale_bu_toggled(GtkWidget * widget, gpointer * data);
static void spec_prefs_to_widgets(void);

/* save_event, called by the save button, saves the preferences to file     
 */
static void save_event(GtkWidget * widget, gpointer data)
{
  rc_file_write();
}


static void ptt_clicked(GtkWidget * widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON(widget)->active == TRUE)
    ptt_on();
  else
    ptt_off();
}


static void key_clicked(GtkWidget * widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON(widget)->active == TRUE)
    key_down();
  else
    key_up();
}

static void close_tx_test_dialog(GtkWidget * widget, gpointer data)
{
  /* make sure PTT and KEY are off */
  ptt_off();
  key_up();

  gtk_widget_destroy(widget);
}


void tx_test_dialog(GtkWidget * widget, gpointer data)
{
  GtkWidget *temp_bu, *ptt_bu, *key_bu;
  GtkWidget *te_fr;
  GtkWidget *tmp_hb;

  if (tx_test_window && tx_test_window->window) {
    gtk_widget_map(tx_test_window);
    gdk_window_raise(tx_test_window->window);
    return;
  }
  tx_test_window = gtk_dialog_new();
  g_signal_connect(G_OBJECT(tx_test_window), "destroy", G_CALLBACK(gtk_widget_destroyed), &tx_test_window);

  /* setup window properties */
  gtk_window_set_wmclass(GTK_WINDOW(tx_test_window), "txtest", "glfer");
  gtk_window_set_title(GTK_WINDOW(tx_test_window), "TX test");
  gtk_window_set_policy(GTK_WINDOW(tx_test_window), FALSE, FALSE, FALSE);
  gtk_window_position(GTK_WINDOW(tx_test_window), GTK_WIN_POS_NONE);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(tx_test_window)->vbox), 5);

  /* TX test frame */
  te_fr = gtk_frame_new("Test");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(tx_test_window)->vbox), te_fr, FALSE, FALSE, 10);
  gtk_widget_show(te_fr);

  /* TX test hbox */
  tmp_hb = gtk_hbox_new(FALSE, 5);
  gtk_container_add(GTK_CONTAINER(te_fr), tmp_hb);
  gtk_widget_show(tmp_hb);
  /* PTT button */
  ptt_bu = gtk_check_button_new_with_label("PTT");
  gtk_tooltips_set_tip(glfer.tt, ptt_bu, "Toggle PTT", NULL);
  gtk_box_pack_start(GTK_BOX(tmp_hb), ptt_bu, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(ptt_bu), "clicked", G_CALLBACK(ptt_clicked), NULL);
  gtk_widget_show(ptt_bu);
  /* KEY button */
  key_bu = gtk_check_button_new_with_label("KEY");
  gtk_tooltips_set_tip(glfer.tt, key_bu, "Toggle KEY", NULL);
  gtk_box_pack_start(GTK_BOX(tmp_hb), key_bu, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(key_bu), "clicked", G_CALLBACK(key_clicked), NULL);
  gtk_widget_show(key_bu);

  /* OK button */
  temp_bu = gtk_button_new_with_label("Ok");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Close window", NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(close_tx_test_dialog), GTK_OBJECT(tx_test_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(tx_test_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* make sure PTT and KEY are off */
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ptt_bu), FALSE);
  ptt_off();
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(key_bu), FALSE);
  key_up();

  gtk_widget_show(tx_test_window);
}

/* compound_entry, a convenience function to create a label and an entry in
 * a horizontal box, and pack it into the vbox provided                      
 */
static void compound_entry(gchar * title, GtkWidget ** entry, GtkWidget * vbox)
{
  GtkWidget *hbox, *label;

  hbox = gtk_hbox_new(FALSE, 0);
  *entry = gtk_entry_new();
  //gtk_widget_set_usize(*entry, 75, -1);
  gtk_box_pack_end(GTK_BOX(hbox), *entry, FALSE, FALSE, 0);
  label = gtk_label_new(title);
  gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(label);
  gtk_widget_show(*entry);
  gtk_widget_show(hbox);
}


/* prefs_to_widgets, takes the information
 * in the prefs struct, and puts it into the appropriate widgets
 */
static void spec_prefs_to_widgets(void)
{
  gchar *tmp_str;

  tmp_str = g_strdup_printf("%.1f", opt.offset_freq);
  gtk_entry_set_text(GTK_ENTRY(offset_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.thr_level);
  gtk_entry_set_text(GTK_ENTRY(level_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.max_level_db);       
  gtk_entry_set_text(GTK_ENTRY(max_level_en), tmp_str);      
  g_free(tmp_str);                                           

  tmp_str = g_strdup_printf("%.1f", opt.min_level_db);       
  gtk_entry_set_text(GTK_ENTRY(min_level_en), tmp_str);      
  g_free(tmp_str);                                           

  tmp_str = g_strdup_printf("%i", opt.avgsamples);           
  gtk_entry_set_text(GTK_ENTRY(avgsamples_en), tmp_str);     
  g_free(tmp_str);                                           

  tmp_str = g_strdup_printf("%f", opt.min_avgband);          
  gtk_entry_set_text(GTK_ENTRY(minavgband_en), tmp_str);     
  g_free(tmp_str);                                           

  tmp_str = g_strdup_printf("%f", opt.max_avgband);          
  gtk_entry_set_text(GTK_ENTRY(maxavgband_en), tmp_str);     
  g_free(tmp_str);                                           

  /* FFT */
  tmp_str = g_strdup_printf("%.2f", opt.data_blocks_overlap * 100.0);
  gtk_entry_set_text(GTK_ENTRY(data_blocks_overlap_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.3f", opt.display_update_time);
  gtk_entry_set_text(GTK_ENTRY(display_update_en), tmp_str);
  g_free(tmp_str);  

  /* keep this after the preceding two, since it will trigger an update of the related entries... */
  tmp_str = g_strdup_printf("%i", opt.data_block_size);
  gtk_entry_set_text(GTK_ENTRY(data_block_size_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%f", opt.limiter_a);
  gtk_entry_set_text(GTK_ENTRY(limiter_a_en), tmp_str);
  g_free(tmp_str);

  if (opt.enable_limiter == 1)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(hendrix_bu), TRUE);
  else
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(hendrix_bu), FALSE);

  /* MTM */
  tmp_str = g_strdup_printf("%.1f", opt.mtm_w);
  gtk_entry_set_text(GTK_ENTRY(mtm_w_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%i", opt.mtm_k);
  gtk_entry_set_text(GTK_ENTRY(mtm_k_en), tmp_str);
  g_free(tmp_str);

  /* HPARMA */
  tmp_str = g_strdup_printf("%i", opt.hparma_t);
  gtk_entry_set_text(GTK_ENTRY(hparma_t_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%i", opt.hparma_p_e);
  gtk_entry_set_text(GTK_ENTRY(hparma_p_e_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%i", opt.sample_rate);
  gtk_entry_set_text(GTK_ENTRY(sample_rate_en), tmp_str);
  g_free(tmp_str);

  /* LMP */
  tmp_str = g_strdup_printf("%i", opt.lmp_av);
  gtk_entry_set_text(GTK_ENTRY(lmp_av_en), tmp_str);
  g_free(tmp_str);

  /* Display */
  if (opt.scale_type == SCALE_LIN)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lin_scale_bu), TRUE);
  else if (opt.scale_type == SCALE_LIN_MAX0)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lin_scale_max0_bu), TRUE);  
  else if (opt.scale_type == SCALE_LOG)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(log_scale_bu), TRUE);
  else {
    g_print("opt.scale_type = %i\n", opt.scale_type);
    g_assert_not_reached();
  }
  
  switch (opt.averaging) 
  {
    case AVG_SUMAVG:
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(avg_sumavg_bu), TRUE);
      break;
    case AVG_PLAIN:
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(avg_plain_bu), TRUE);
      break;
    case AVG_SUMEXTREME:
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(avg_sumextreme_bu), TRUE);
      break;
    case NO_AVG:
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(noavg_bu), TRUE);
      break;
  }
  
  if (opt.autoscale == 1)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(autoscale_bu), TRUE);
  else
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(autoscale_bu), FALSE);


}

static void avg_reset(GtkWidget * widget, gpointer data)
{
  gchar *tmp_str;
  int   tmp_int;
  
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(avgsamples_en), 0, -1); 
  tmp_int = atoi(tmp_str);
  opt.avgsamples = tmp_int;
  delete_avg(&avgdata);
  alloc_avg(&avgdata,opt.data_block_size,opt.avgsamples);
  g_free(tmp_str);  
  glfer.avgfill = 0; // reset fill ratio of averaging buffer
}


/* widgets_to_prefs, callback for ok button,
 * puts the information in the widgets into the prefs struct
 */
static void spec_widgets_to_prefs(GtkWidget * widget, gpointer data)
{
  gchar *tmp_str;
  int   tmp_int;
  float tmp_float; 

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(offset_en), 0, -1);
  opt.offset_freq = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(level_en), 0, -1);
  opt.thr_level = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(max_level_en), 0, -1); 
  opt.max_level_db = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(min_level_en), 0, -1); 
  opt.min_level_db = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(avgsamples_en), 0, -1); 
  tmp_int = atoi(tmp_str);
  g_free(tmp_str);
  if (tmp_int != opt.avgsamples) // FIXME: ahould call alloc_avg also at first average usage (first call)
  {
    opt.avgsamples = tmp_int;
    delete_avg(&avgdata);
    alloc_avg(&avgdata,opt.data_block_size,opt.avgsamples);    
    tmp_str = g_strdup_printf("Integration gain %.1f dB", 10.0*log10(((float) opt.avgsamples)*(1.0-opt.data_blocks_overlap)));
    gtk_label_set_text(GTK_LABEL(avgintdb_la), tmp_str);
    g_free(tmp_str); 
    glfer.avgfill = 0; // reset ratio of average buffer full 
    glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-opt.data_blocks_overlap);
    tmp_str = g_strdup_printf("Avg time %2.2f s", glfer.avgtime);
    gtk_label_set_text(GTK_LABEL(avgtime_la), tmp_str);
    g_free(tmp_str); 
  } 

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(minavgband_en), 0, -1); 
  opt.min_avgband = atof(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(maxavgband_en), 0, -1); 
  opt.max_avgband = atof(tmp_str);

  /* FFT */
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(data_block_size_en), 0, -1);
  opt.data_block_size = atoi(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(data_blocks_overlap_en), 0, -1);
  opt.data_blocks_overlap = atof(tmp_str) / 100.0;
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(display_update_en), 0, -1);
  opt.display_update_time = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(limiter_a_en), 0, -1);
  opt.limiter_a = atof(tmp_str);
  g_free(tmp_str);

  if (GTK_TOGGLE_BUTTON(hendrix_bu)->active == TRUE)
    opt.enable_limiter = 1;
  else
    opt.enable_limiter = 0;
   
  /* MTM */
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(mtm_w_en), 0, -1);
  opt.mtm_w = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(mtm_k_en), 0, -1);
  opt.mtm_k = atoi(tmp_str);
  g_free(tmp_str);

  /* HPARMA */
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(hparma_t_en), 0, -1);
  opt.hparma_t = atoi(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(hparma_p_e_en), 0, -1);
  opt.hparma_p_e = atoi(tmp_str);
  g_free(tmp_str);
   
  /* LMP */
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(lmp_av_en), 0, -1);
  opt.lmp_av = atoi(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(sample_rate_en), 0, -1);
  tmp_int = atoi(tmp_str);
  g_free(tmp_str);
  if (tmp_int != opt.sample_rate) // EG changed?
  {
    opt.sample_rate = tmp_int;
    tmp_str = g_strdup_printf("Resulting bin size %3.3f Hz", (float) opt.sample_rate / (float) opt.data_block_size);            // update bin size label
    gtk_label_set_text(GTK_LABEL(bin_size_la), tmp_str);
    g_free(tmp_str); 
  }

  if (GTK_TOGGLE_BUTTON(lin_scale_bu)->active)
    opt.scale_type = SCALE_LIN;
  else if (GTK_TOGGLE_BUTTON(lin_scale_max0_bu)->active) 
    opt.scale_type = SCALE_LIN_MAX0;
  else if (GTK_TOGGLE_BUTTON(log_scale_bu)->active)
    opt.scale_type = SCALE_LOG;
  else if (GTK_TOGGLE_BUTTON(log_scale_max0_bu)->active) 
    opt.scale_type = SCALE_LOG_MAX0;
  else
    g_assert_not_reached();

  if (GTK_TOGGLE_BUTTON(avg_sumavg_bu)->active)          // EG averaging mode
    opt.averaging = AVG_SUMAVG;
  else if (GTK_TOGGLE_BUTTON(noavg_bu)->active)
    opt.averaging = NO_AVG;
  else if (GTK_TOGGLE_BUTTON(avg_plain_bu)->active)
    opt.averaging = AVG_PLAIN;
  else if (GTK_TOGGLE_BUTTON(avg_sumextreme_bu)->active)
    opt.averaging = AVG_SUMEXTREME;
  else
    g_assert_not_reached();

  if (GTK_TOGGLE_BUTTON(autoscale_bu)->active == TRUE)   // EG autoscale mode
    opt.autoscale = 1;
  else
    opt.autoscale = 0;

  change_params(glfer.current_mode);
}


static void palette_sel_changed(gpointer dummy, guint callback_action, GtkWidget * widget)
{
  opt.palette = callback_action; 
  set_palette(callback_action);
}

static void fft_windows_sel_changed(gpointer dummy, guint callback_action, GtkWidget * widget)
{
  opt.window_type = callback_action;
  change_params(glfer.current_mode);
}


void spec_settings_dialog(GtkWidget * widget, gpointer data)
{
  gint i;
  gchar *tmp_str;
  GtkWidget *nb;
  GtkWidget *temp_la;
  GtkWidget *temp_bu;
  GtkWidget *tmp_vb, *tb_vb, *tmp_hb;
  GtkWidget *pa_fr, *ty_fr;
  GtkItemFactory *item_factory = NULL;
  GtkItemFactoryEntry entry;
  char *menu_title = "<Palette_Types>";
  GtkWidget *menu = NULL;
  GtkWidget *option_menu = NULL;
  GSList *scale_type_gr, *avg_type_gr;

  if (spec_window && spec_window->window) {
    gtk_widget_map(spec_window);
    gdk_window_raise(spec_window->window);
    return;
  }
  spec_window = gtk_dialog_new();
  g_signal_connect(G_OBJECT(spec_window), "destroy", G_CALLBACK(gtk_widget_destroyed), &spec_window);

  /* setup window properties */
  gtk_window_set_wmclass(GTK_WINDOW(spec_window), "specprefs", "glfer");
  gtk_window_set_title(GTK_WINDOW(spec_window), "Spectrogram preferences");
  gtk_window_set_policy(GTK_WINDOW(spec_window), FALSE, FALSE, FALSE);
  gtk_window_position(GTK_WINDOW(spec_window), GTK_WIN_POS_NONE);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(spec_window)->vbox), 5);

  nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->vbox), nb, TRUE, TRUE, 0);
  gtk_widget_show(nb);

  /* Input data frame */
  pa_fr = gtk_frame_new("Input data");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("Input data");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);

  /* Input data vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  /* Input data entries */
  /* number of points */
  compound_entry("Points :", &data_block_size_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, data_block_size_en, "Number of points for the data blocks to be analyzed", NULL);
  /* we connect this function here so when the block size is changed the display update interval is kept constant */
  g_signal_connect(G_OBJECT(data_block_size_en), "changed", G_CALLBACK(data_block_size_en_changed), NULL);
  /* overlap between FFT blocks */
  compound_entry("Overlap [%]:", &data_blocks_overlap_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, data_blocks_overlap_en, "Overlap between successive data blocks [0-100%]", NULL);
  g_signal_connect(G_OBJECT(data_blocks_overlap_en), "changed", G_CALLBACK(data_blocks_overlap_en_changed), NULL);

  /* display update time */
  compound_entry("Update interval [s]:", &display_update_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, display_update_en, "Interval between display updates", NULL);
  g_signal_connect(G_OBJECT(display_update_en), "changed", G_CALLBACK(display_update_en_changed), NULL);

  // resulting bin size 0.3.5 EG
  tmp_str = g_strdup_printf("Resulting bin size %3.3f Hz", (float) opt.sample_rate / (float) opt.data_block_size);
  bin_size_la = gtk_label_new(tmp_str);
  gtk_box_pack_start(GTK_BOX(tmp_vb), bin_size_la, TRUE, TRUE, 0);
  gtk_widget_show(bin_size_la);

  /* RA9MB nonlinear processing parameter */
  compound_entry("RA9MB A parameter :", &limiter_a_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, limiter_a_en, "EXPERIMENTAL: RA9MB nonlinear processing A parameter. 0.0 is conventional periodogram, otherwise 0.001 is a good starting point", NULL);

  /* "Hendrixizer" button */
  hendrix_bu = gtk_check_button_new_with_label("Hendrixizer (limiter)");
  gtk_tooltips_set_tip(glfer.tt, hendrix_bu, "Enable strong limiting on input signal", NULL);
  gtk_box_pack_start(GTK_BOX(tmp_vb), hendrix_bu, TRUE, TRUE, 0);
  gtk_widget_show(hendrix_bu);

  /* FFT frame */
  pa_fr = gtk_frame_new("FFT");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("FFT");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);

  /* FFT vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  /* FFT entries */
  /* FFT window selection */
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, menu_title, NULL);
  for (i = 0; i < num_fft_windows; i++) {
    entry.path = fft_windows[i].name;
    entry.accelerator = NULL;
    entry.callback = fft_windows_sel_changed;
    entry.callback_action = fft_windows[i].type;;
    entry.item_type = NULL;
    gtk_item_factory_create_item(item_factory, &entry, NULL, 1);
  }
  menu = gtk_item_factory_get_widget(item_factory, menu_title);
  /* create the option menu for the FFT data window type list */
  option_menu = gtk_option_menu_new();
  gtk_tooltips_set_tip(glfer.tt, option_menu, "FFT data window type", NULL);
  /* provide the popup menu */
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  /* add label */
  temp_la = gtk_label_new("Window :");
  gtk_widget_show(temp_la);
  tmp_hb = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(tmp_hb);
  gtk_box_pack_start(GTK_BOX(tmp_vb), tmp_hb, FALSE, FALSE, 5);
  gtk_box_pack_end(GTK_BOX(tmp_hb), option_menu, FALSE, FALSE, 0);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), opt.window_type); 
  gtk_box_pack_end(GTK_BOX(tmp_hb), temp_la, FALSE, FALSE, 0);
  gtk_widget_show(option_menu);

  /* MTM frame */
  pa_fr = gtk_frame_new("MTM");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("MTM");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);

  /* MTM vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  /* MTM entries */
  compound_entry("Relative bandwidth :", &mtm_w_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, mtm_w_en, "Window relative bandwidth (usually between 1.0 and 4.0)", NULL);

  compound_entry("Number of windows :", &mtm_k_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, mtm_k_en, "Number of windows (tapers) to use for a single block", NULL);

  /* HPARMA frame */
  pa_fr = gtk_frame_new("HPARMA");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("HPARMA");

  /* HPARMA vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);
  compound_entry("t   :", &hparma_t_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, hparma_t_en, "Number of equations used to solve the extended Yule-Walker equation", NULL);
  compound_entry("p_e :", &hparma_p_e_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, hparma_p_e_en, "Number of equations parameters in the AR model", NULL);

  /* LMP frame */
  pa_fr = gtk_frame_new("LMP");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("LMP");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);

  /* LMP vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  /* LMP entries */
  compound_entry("Averaged data blocks :", &lmp_av_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, lmp_av_en, "Number of data blocks for the internal average", NULL);

  /* Acquisition frame */
  pa_fr = gtk_frame_new("Acquisition");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("Acquisition");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);

  /* Acquisition vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);
  /* sample rate */
  compound_entry("Sample rate [Hz] :", &sample_rate_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, sample_rate_en, "Soundcard sample rate", NULL);

  /* Display frame */
  pa_fr = gtk_frame_new("Display");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("Display");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);
  /* Display vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);
  /* offset entry */
  compound_entry("Offset [Hz] :", &offset_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, offset_en, "Spectrogram frequency offset", NULL);
  /* threshold level entry */
  compound_entry("Threshold level [%] :", &level_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, level_en, "Display threshold level [0-99.9]", NULL);
  // max level entry 0.3.5 EG
  compound_entry("Max level [dB] :", &max_level_en, tmp_vb);              
  gtk_tooltips_set_tip(glfer.tt, max_level_en, "Maximum display level [dB]", NULL);
  // min level entry 0.3.5 EG
  compound_entry("Min level [dB] :", &min_level_en, tmp_vb);              
  gtk_tooltips_set_tip(glfer.tt, min_level_en, "Minimum display level [dB]", NULL);
  // autoscale button 0.3.5 EG
  autoscale_bu = gtk_check_button_new_with_label("Autoscale");
  gtk_tooltips_set_tip(glfer.tt, autoscale_bu, "set autoscale display mode, max and min are not used then", NULL);
  gtk_box_pack_start(GTK_BOX(tmp_vb), autoscale_bu, TRUE, TRUE, 0);
  /* whan autoscale is enabled, gray out min and max fields */
  g_signal_connect(G_OBJECT(autoscale_bu), "toggled", G_CALLBACK(autoscale_bu_toggled), NULL);
  gtk_widget_show(autoscale_bu);
  
  /* scale type frame */
  ty_fr = gtk_frame_new("Scale");
  gtk_box_pack_start(GTK_BOX(tmp_vb), ty_fr, FALSE, FALSE, 10);
  gtk_widget_show(ty_fr);
  /* Mode hbox */
  tmp_hb = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(ty_fr), tmp_hb);
  gtk_widget_show(tmp_hb);
  /* transmissione mode radio buttons */
  log_scale_bu = gtk_radio_button_new_with_label(NULL, "Log");
  gtk_tooltips_set_tip(glfer.tt, log_scale_bu, "Set logarithmic scale", NULL);
  scale_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(log_scale_bu));
  log_scale_max0_bu = gtk_radio_button_new_with_label(scale_type_gr, "Log Max");
  gtk_tooltips_set_tip(glfer.tt, log_scale_max0_bu, "Set logarithmic scale with maximum set at 0dB (sumavg and sumextreme only)", NULL);
  scale_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(log_scale_max0_bu));
  lin_scale_bu = gtk_radio_button_new_with_label(scale_type_gr, "Lin");
  gtk_tooltips_set_tip(glfer.tt, lin_scale_bu, "Set linear scale", NULL);
  scale_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(lin_scale_bu));
  lin_scale_max0_bu = gtk_radio_button_new_with_label(scale_type_gr, "Lin Max");
  gtk_tooltips_set_tip(glfer.tt, lin_scale_max0_bu, "Set linear scale with maximum set at 0dB (sumavg and sumextreme only)", NULL);
  scale_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(lin_scale_max0_bu));
  gtk_box_pack_start(GTK_BOX(tmp_hb), log_scale_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), log_scale_max0_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), lin_scale_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), lin_scale_max0_bu, TRUE, TRUE, 0);
  gtk_widget_show(lin_scale_bu);
  gtk_widget_show(log_scale_bu);
  gtk_widget_show(lin_scale_max0_bu);
  gtk_widget_show(log_scale_max0_bu);
  /* palette frame */
  pa_fr = gtk_frame_new("Palette");
  gtk_box_pack_start(GTK_BOX(tmp_vb), pa_fr, FALSE, FALSE, 10);
  gtk_widget_show(pa_fr);
  /* palette selection vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, menu_title, NULL);
  for (i = 0; i < num_palettes; i++) {
    entry.path = palettes[i].name;
    entry.accelerator = NULL;
    entry.callback = palette_sel_changed;
    entry.callback_action = palettes[i].type;
    entry.item_type = NULL;
    gtk_item_factory_create_item(item_factory, &entry, NULL, 1);
  }
  menu = gtk_item_factory_get_widget(item_factory, menu_title);
  /* create the option menu for the file type list */
  option_menu = gtk_option_menu_new();
  gtk_tooltips_set_tip(glfer.tt, option_menu, "Spectrogram palette", NULL);
  /* provide the popup menu */
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), opt.palette); 
  gtk_box_pack_start(GTK_BOX(tmp_vb), option_menu, FALSE, FALSE, 0);
  gtk_widget_show(option_menu);

  // Averaging frame 0.3.5 EG
  pa_fr = gtk_frame_new("Averaging");
  gtk_widget_show(pa_fr);
  temp_la = gtk_label_new("Averaging");
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), pa_fr, temp_la);
  // Display vbox 
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);
  // Averaging selection frame
  ty_fr = gtk_frame_new("Mode");
  gtk_box_pack_start(GTK_BOX(tmp_vb), ty_fr, FALSE, FALSE, 10);
  gtk_widget_show(ty_fr);
  // Mode hbox 
  tmp_hb = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(ty_fr), tmp_hb);
  gtk_widget_show(tmp_hb);
  
  // averaging mode radio buttons 0.3.5 EG
  
  avg_sumavg_bu = gtk_radio_button_new_with_label(NULL, "Sumavg");
  gtk_tooltips_set_tip(glfer.tt, avg_sumavg_bu, "Set sum scaled from average to max mode", NULL);
  avg_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(avg_sumavg_bu));
  
  avg_plain_bu = gtk_radio_button_new_with_label(avg_type_gr, "Plain");
  gtk_tooltips_set_tip(glfer.tt, avg_plain_bu, "Set plain average i.e. sum scaled by memory depth mode", NULL);
  avg_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(avg_plain_bu));
  
  avg_sumextreme_bu = gtk_radio_button_new_with_label(avg_type_gr, "Sumextreme");
  gtk_tooltips_set_tip(glfer.tt, avg_sumextreme_bu, "Set sum scaled to the extremes average mode", NULL);
  avg_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(avg_sumextreme_bu));
  
  noavg_bu = gtk_radio_button_new_with_label(avg_type_gr, "NoAvg");
  gtk_tooltips_set_tip(glfer.tt, noavg_bu, "Set no averaging mode", NULL);
  avg_type_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(noavg_bu));
  
  gtk_box_pack_start(GTK_BOX(tmp_hb), avg_sumavg_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), avg_plain_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), avg_sumextreme_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), noavg_bu, TRUE, TRUE, 0);

  gtk_widget_show(avg_sumavg_bu);
  gtk_widget_show(avg_plain_bu);
  gtk_widget_show(avg_sumextreme_bu);
  gtk_widget_show(noavg_bu);

  // number of samples to average over
  compound_entry("#Samples :", &avgsamples_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, avgsamples_en, "Number of samples to average over", NULL);
  
  // lower cutoff frequency for sum extreme mode
  compound_entry("Low cutoff [Hz] :", &minavgband_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, minavgband_en, "lower cutoff frequency for sum extreme mode", NULL);
  
  // higher cutoff frequency for sum extreme mode
  compound_entry("High cutoff [Hz] :", &maxavgband_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, maxavgband_en, "higher cutoff frequency for sum extreme mode", NULL);
  
  // Integration gain
  tmp_str = g_strdup_printf("Integration gain %.1f dB", 10.0*log10(((float) opt.avgsamples)*(1.0-opt.data_blocks_overlap)));
  avgintdb_la = gtk_label_new(tmp_str);
  gtk_box_pack_start(GTK_BOX(tmp_vb), avgintdb_la, TRUE, TRUE, 0);
  gtk_widget_show(avgintdb_la);

  // resulting integration time
  glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-opt.data_blocks_overlap);
  tmp_str = g_strdup_printf("Avg time %2.2f s", glfer.avgtime);
  avgtime_la = gtk_label_new(tmp_str);
  gtk_box_pack_start(GTK_BOX(tmp_vb), avgtime_la, TRUE, TRUE, 0);
  gtk_widget_show(avgtime_la);

  /* button box */
  tb_vb = gtk_vbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->vbox), tb_vb, TRUE, TRUE, 0);
  gtk_widget_show(tb_vb);

  /* OK button */
  temp_bu = gtk_button_new_with_label("Ok");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Accept settings and close window", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(spec_widgets_to_prefs), NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(spec_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* Apply button */
  temp_bu = gtk_button_new_with_label("Apply");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Accept settings and leave settings window open", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(spec_widgets_to_prefs), NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* Save button */
  temp_bu = gtk_button_new_with_label("Save");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Save settings", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(spec_widgets_to_prefs), NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(save_event), NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* Cancel button */
  temp_bu = gtk_button_new_with_label("Cancel");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Close window", NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(spec_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  // Reset memory button 
  temp_bu = gtk_button_new_with_label("AvgRst");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Reset averaging memory", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(avg_reset), NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spec_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* load current values */
  spec_prefs_to_widgets();

  gtk_widget_show(spec_window);
}

static void autoscale_bu_toggled(GtkWidget * widget, gpointer * data)
{
  if (GTK_TOGGLE_BUTTON(autoscale_bu)->active == TRUE) {
    gtk_widget_set_sensitive(max_level_en, FALSE);
    gtk_widget_set_sensitive(min_level_en, FALSE);
  } else {
    gtk_widget_set_sensitive(max_level_en, TRUE);
    gtk_widget_set_sensitive(min_level_en, TRUE);
  }
}

static void enable_sidetone(GtkWidget * widget, gpointer data)
{
  //printf("file: %s\tline: %d\tfunc: %s\t value: %i\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, GTK_TOGGLE_BUTTON(widget)->active);
  /* enable/disable the sidetone entry according to the check button status */
  //gtk_widget_set_sensitive(averages_en, GTK_TOGGLE_BUTTON(widget)->active);
  opt.sidetone = GTK_TOGGLE_BUTTON(widget)->active;
}


static void qrss_prefs_to_widgets(void)
{
  gchar *tmp_str;

  tmp_str = g_strdup_printf("%.1f", opt.dot_time);
  gtk_entry_set_text(GTK_ENTRY(dot_time_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.ptt_delay);
  gtk_entry_set_text(GTK_ENTRY(ptt_delay_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.sidetone_freq);
  gtk_entry_set_text(GTK_ENTRY(sidetone_fr_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.dfcw_dot_freq);
  gtk_entry_set_text(GTK_ENTRY(dfcw_dot_fr_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.dfcw_dash_freq);
  gtk_entry_set_text(GTK_ENTRY(dfcw_dash_fr_en), tmp_str);
  g_free(tmp_str);

  tmp_str = g_strdup_printf("%.1f", opt.beacon_pause);
  gtk_entry_set_text(GTK_ENTRY(be_pause_en), tmp_str);
  g_free(tmp_str);
  
  if (opt.tx_mode == QRSS)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(qrss_bu), TRUE);
  else if (opt.tx_mode == DFCW)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dfcw_bu), TRUE);
  else {
    g_print("opt.tx_mode = %i\n", opt.tx_mode);
    g_assert_not_reached();
  }

  if (opt.beacon_tx_pause == FALSE)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(be_keyup_bu), TRUE);
  else if (opt.beacon_tx_pause == TRUE)
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(be_keydown_bu), TRUE);
  else {
    g_print("opt.beacon_tx_pause = %i\n", opt.beacon_tx_pause);
    g_assert_not_reached();
  }
}


static void qrss_widgets_to_prefs(GtkWidget * widget, gpointer data)
{
  gchar *tmp_str;

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(dot_time_en), 0, -1);
  opt.dot_time = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(ptt_delay_en), 0, -1);
  opt.ptt_delay = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(sidetone_fr_en), 0, -1);
  opt.sidetone_freq = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(dfcw_dot_fr_en), 0, -1);
  opt.dfcw_dot_freq = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(dfcw_dash_fr_en), 0, -1);
  opt.dfcw_dash_freq = atof(tmp_str);
  g_free(tmp_str);

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(be_pause_en), 0, -1);
  opt.beacon_pause = atof(tmp_str);
  g_free(tmp_str);
  
  if (GTK_TOGGLE_BUTTON(be_keyup_bu)->active)
    opt.beacon_tx_pause = FALSE;
  else if (GTK_TOGGLE_BUTTON(be_keydown_bu)->active)
    opt.beacon_tx_pause = TRUE;
  else
    g_assert_not_reached();

  if (GTK_TOGGLE_BUTTON(qrss_bu)->active)
    opt.tx_mode = QRSS;
  else if (GTK_TOGGLE_BUTTON(dfcw_bu)->active)
    opt.tx_mode = DFCW;
  else
    g_assert_not_reached();

  /* update timers shown in the message transmission window, if it exists */
  update_time_indications();
}


static void data_block_size_en_changed(GtkWidget * widget, gpointer * data)
{
  gchar *tmp_str;
  int tmp_i;
  
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(data_block_size_en), 0, -1);
  tmp_i = atoi(tmp_str);
  g_free(tmp_str);

  opt.data_block_size = tmp_i;
  /* update bin size label */
  tmp_str = g_strdup_printf("Resulting bin size %3.3f Hz", (float) opt.sample_rate / (float) tmp_i);
  gtk_label_set_text(GTK_LABEL(bin_size_la), tmp_str);
  g_free(tmp_str); 
  /* update average time label */
  tmp_str = g_strdup_printf("Avg time %2.2f s", glfer.avgtime);
  gtk_label_set_text(GTK_LABEL(avgtime_la), tmp_str);
  g_free(tmp_str); 
  
  /* keep the overlap constant and change the update interval */
  opt.display_update_time = opt.data_block_size * (1.0 - opt.data_blocks_overlap) / opt.sample_rate;
  tmp_str = g_strdup_printf("%.3f", opt.display_update_time);
  gtk_signal_handler_block_by_func(GTK_OBJECT(display_update_en), G_CALLBACK(display_update_en_changed), data);
  gtk_entry_set_text(GTK_ENTRY(display_update_en), tmp_str);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(display_update_en), G_CALLBACK(display_update_en_changed), data);
  g_free(tmp_str);
}


static void data_blocks_overlap_en_changed(GtkWidget * widget, gpointer * data)
{
  gchar *tmp_str;
  float tmp_f;

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(data_blocks_overlap_en), 0, -1);
  tmp_f = atof(tmp_str) / 100.0;
  g_free(tmp_str);

  /* FIXME: the following check should be improved... */
  if ((tmp_f < 0) || (tmp_f >= 1.0)) {
    show_message("Overlap must be between 0 [included] and 100 [excluded]");
    /* reset entry to the current valid value */
    tmp_str = g_strdup_printf("%.2f", opt.data_blocks_overlap * 100.0);
    gtk_entry_set_text(GTK_ENTRY(data_blocks_overlap_en), tmp_str);
    gtk_editable_select_region(GTK_EDITABLE(data_blocks_overlap_en), 0, -1);
    g_free(tmp_str);
    
    return;
  }

  opt.data_blocks_overlap = tmp_f;
  opt.display_update_time = (1.0 - tmp_f) * opt.data_block_size / opt.sample_rate;
  tmp_str = g_strdup_printf("%.3f", opt.display_update_time);
  gtk_signal_handler_block_by_func(GTK_OBJECT(display_update_en), G_CALLBACK(display_update_en_changed), data);
  gtk_entry_set_text(GTK_ENTRY(display_update_en), tmp_str);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(display_update_en), G_CALLBACK(display_update_en_changed), data);
  g_free(tmp_str);

  /* update integration gain and average time labels */
  tmp_str = g_strdup_printf("Integration gain %.1f dB", 10.0*log10(((float) opt.avgsamples)*(1.0-tmp_f)));
  gtk_label_set_text(GTK_LABEL(avgintdb_la), tmp_str);
  g_free(tmp_str); 
  glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-tmp_f);
  tmp_str = g_strdup_printf("Avg time %2.2f s", glfer.avgtime);
  gtk_label_set_text(GTK_LABEL(avgtime_la), tmp_str);
  g_free(tmp_str); 
}


static void display_update_en_changed(GtkWidget * widget, gpointer * data)
{
  gchar *tmp_str;
  float tmp_update_time, tmp_overlap;

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(display_update_en), 0, -1);
  tmp_update_time = atof(tmp_str);
  g_free(tmp_str);
   
  /* compute resulting overlap */
  tmp_overlap = 1.0 - opt.sample_rate * tmp_update_time / opt.data_block_size;

  if ((tmp_overlap < 0) || (tmp_overlap >= 1.0)) {
    /* resulting overlap not acceptable */
  } else {
    opt.display_update_time = tmp_update_time;
    opt.data_blocks_overlap = tmp_overlap;
    tmp_str = g_strdup_printf("%.1f", opt.data_blocks_overlap * 100.0);
    gtk_signal_handler_block_by_func(GTK_OBJECT(data_blocks_overlap_en), G_CALLBACK(data_blocks_overlap_en_changed), data);
    gtk_entry_set_text(GTK_ENTRY(data_blocks_overlap_en), tmp_str);
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(data_blocks_overlap_en), G_CALLBACK(data_blocks_overlap_en_changed), data);
    g_free(tmp_str);
  }
}


static void wpm_en_changed(GtkWidget * widget, gpointer * data)
{
  gchar *tmp_str;
  float tmp_f;

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(wpm_en), 0, -1);
  tmp_f = atof(tmp_str);
  g_free(tmp_str);
  opt.dot_time = 1000.0 * 60.0 / (tmp_f * 50);
  tmp_str = g_strdup_printf("%.1f", opt.dot_time);
  gtk_signal_handler_block_by_func(GTK_OBJECT(dot_time_en), G_CALLBACK(dot_time_en_changed), data);
  gtk_entry_set_text(GTK_ENTRY(dot_time_en), tmp_str);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(dot_time_en), G_CALLBACK(dot_time_en_changed), data);
  g_free(tmp_str);
}


static void dot_time_en_changed(GtkWidget * widget, gpointer * data)
{
  gchar *tmp_str;
  float tmp_f1, tmp_f2;

  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(dot_time_en), 0, -1);
  tmp_f1 = atof(tmp_str);
  g_free(tmp_str);
  tmp_f2 = 60.0 / (tmp_f1 * 50 / 1000.0);
  tmp_str = g_strdup_printf("%.1f", tmp_f2);
  gtk_signal_handler_block_by_func(GTK_OBJECT(wpm_en), G_CALLBACK(wpm_en_changed), data);
  gtk_entry_set_text(GTK_ENTRY(wpm_en), tmp_str);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(wpm_en), G_CALLBACK(wpm_en_changed), data);
  //  gtk_signal_emit_stop_by_name(GTK_OBJECT(wpm_en), "changed");
  g_free(tmp_str);
}


void qrss_settings_dialog(GtkWidget * widget, gpointer data)
{
  GtkWidget *temp_bu;
  GtkWidget *tmp_vb, *tmp_hb;
  GtkWidget *ti_fr, *be_fr, *si_fr, *mo_fr;
  GSList *mode_gr, *be_gr;

  if (qrss_window && qrss_window->window) {
    gtk_widget_map(qrss_window);
    gdk_window_raise(qrss_window->window);
    return;
  }
  qrss_window = gtk_dialog_new();
  g_signal_connect(G_OBJECT(qrss_window), "destroy", G_CALLBACK(gtk_widget_destroyed), &qrss_window);

  /* setup window properties */
  gtk_window_set_wmclass(GTK_WINDOW(qrss_window), "qrssprefs", "glfer");
  gtk_window_set_title(GTK_WINDOW(qrss_window), "QRSS preferences");
  gtk_window_set_policy(GTK_WINDOW(qrss_window), FALSE, FALSE, FALSE);
  gtk_window_position(GTK_WINDOW(qrss_window), GTK_WIN_POS_NONE);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(qrss_window)->vbox), 5);

  /* QRSS/DFCW timings frame */
  ti_fr = gtk_frame_new("QRSS/DFCW timings");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->vbox), ti_fr, FALSE, FALSE, 10);
  gtk_widget_show(ti_fr);
  /* Timing vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(ti_fr), tmp_vb);
  gtk_widget_show(tmp_vb);
  /* Timing entries */
  compound_entry("Dot length [ms] :", &dot_time_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, dot_time_en, "dot time duration", NULL);
  compound_entry("Words per minute [WPM] :", &wpm_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, wpm_en, "keying speed in word per minute", NULL);
  g_signal_connect(G_OBJECT(dot_time_en), "changed", G_CALLBACK(dot_time_en_changed), NULL);
  g_signal_connect(G_OBJECT(wpm_en), "changed", G_CALLBACK(wpm_en_changed), NULL);
  compound_entry("PTT delay [ms] :", &ptt_delay_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, ptt_delay_en, "delay between PTT and KEY", NULL);

  /* Beacon mode timings frame */
  be_fr = gtk_frame_new("Beacon mode timings");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->vbox), be_fr, FALSE, FALSE, 10);
  gtk_widget_show(be_fr);
  /* Timing vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(be_fr), tmp_vb);
  gtk_widget_show(tmp_vb);
  /* Timing entries */
  compound_entry("Pause between messages [s] :", &be_pause_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, be_pause_en, "Pause between beacon transmission", NULL);
  /* beacon pause with key up or down radio buttons */
  be_keyup_bu = gtk_radio_button_new_with_label(NULL, "Key up between pauses");
  gtk_tooltips_set_tip(glfer.tt, be_keyup_bu, "Transmitter OFF between pauses", NULL);
  be_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(be_keyup_bu));
  be_keydown_bu = gtk_radio_button_new_with_label(be_gr, "Key down between pauses");
  gtk_tooltips_set_tip(glfer.tt, be_keydown_bu, "Transmitter ON between pauses", NULL);
  be_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(be_keydown_bu));
  gtk_box_pack_start(GTK_BOX(tmp_vb), be_keyup_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_vb), be_keydown_bu, TRUE, TRUE, 0);
  gtk_widget_show(be_keyup_bu);
  gtk_widget_show(be_keydown_bu);

  /* Sidetone frame */
  si_fr = gtk_frame_new("Sidetone");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->vbox), si_fr, FALSE, FALSE, 10);
  gtk_widget_show(si_fr);

  /* Sidetone hbox */
  tmp_hb = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(si_fr), tmp_hb);
  gtk_widget_show(tmp_hb);
  /* Sound enable check button */
  sn_bu = gtk_check_button_new_with_label("Sound");
  gtk_tooltips_set_tip(glfer.tt, sn_bu, "Enable/disable sound output", NULL);
  gtk_box_pack_start(GTK_BOX(tmp_hb), sn_bu, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(sn_bu), "clicked", G_CALLBACK(enable_sidetone), NULL);
  /* if sidetone is enabled activate the toggle button */
  if (opt.sidetone == TRUE) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sn_bu), TRUE);
  } else {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sn_bu), FALSE);
  }
  gtk_widget_show(sn_bu);
  /* frequencies vbox */
  tmp_vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(tmp_hb), tmp_vb);
  gtk_widget_show(tmp_vb);
  /* Sidetone frequency */
  compound_entry("Frequency [Hz] :", &sidetone_fr_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, sidetone_fr_en, "QRSS sidetone frequency", NULL);
  compound_entry("DFCW dot freq. [Hz] :", &dfcw_dot_fr_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, dfcw_dot_fr_en, "DFCW sidetone dot frequency", NULL);
  compound_entry("DFCW dash freq. [Hz] :", &dfcw_dash_fr_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, dfcw_dash_fr_en, "DFCW sidetone dash frequency", NULL);

  /* Mode frame */
  mo_fr = gtk_frame_new("Mode");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->vbox), mo_fr, FALSE, FALSE, 10);
  gtk_widget_show(mo_fr);
  /* Mode hbox */
  tmp_hb = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(mo_fr), tmp_hb);
  gtk_widget_show(tmp_hb);
  /* transmissione mode radio buttons */
  qrss_bu = gtk_radio_button_new_with_label(NULL, "QRSS");
  gtk_tooltips_set_tip(glfer.tt, qrss_bu, "Set QRSS mode", NULL);
  mode_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(qrss_bu));
  dfcw_bu = gtk_radio_button_new_with_label(mode_gr, "DFCW");
  gtk_tooltips_set_tip(glfer.tt, dfcw_bu, "Set DFCW mode", NULL);
  mode_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(qrss_bu));
  gtk_box_pack_start(GTK_BOX(tmp_hb), qrss_bu, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(tmp_hb), dfcw_bu, TRUE, TRUE, 0);
  gtk_widget_show(qrss_bu);
  gtk_widget_show(dfcw_bu);

  /* buttons */
  temp_bu = gtk_button_new_with_label("Ok");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Accept settings and close window", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(qrss_widgets_to_prefs), NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(qrss_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  temp_bu = gtk_button_new_with_label("Save");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Save settings", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(qrss_widgets_to_prefs), NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(save_event), NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  temp_bu = gtk_button_new_with_label("Cancel");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Close window", NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(qrss_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(qrss_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* load current values */
  qrss_prefs_to_widgets();

  gtk_widget_show(qrss_window);
}


static void port_change_device(GtkWidget * widget, gpointer data)
{
  int dev_status = -1;		/* avoid warning about uninitialized variable */
  char *action = (char *) data;

  /* close currently used port */
  if (glfer.init_done == TRUE)
    switch (opt.device_type) {
    case DEV_SERIAL:
      close_serial_port();
      break;
    case DEV_PARALLEL:
      /* no need to close parallel port */
      break;
    default:
      break;
    }

  if (GTK_TOGGLE_BUTTON(serial_bu)->active) {	/* serial device selected */
    opt.device_type = DEV_SERIAL;
    FREE_MAYBE(opt.ctrl_device);
    opt.ctrl_device = gtk_editable_get_chars(GTK_EDITABLE(control_port_en), 0, -1);
    dev_status = open_serial_port(opt.ctrl_device);
  } else if (GTK_TOGGLE_BUTTON(parallel_bu)->active) {
    opt.device_type = DEV_PARALLEL;
    FREE_MAYBE(opt.ctrl_device);
    opt.ctrl_device = gtk_editable_get_chars(GTK_EDITABLE(control_port_en), 0, -1);
    dev_status = open_parport(opt.ctrl_device);
  } else {
    g_assert_not_reached();
  }

  if (dev_status == -1) {
    /* show warning message */
    show_message("Open %s failed !\n" "Please go to the Settings/Port menu and select a control\n" "device to enable transmission functions.", opt.ctrl_device);
    glfer.init_done = FALSE;
    gtk_widget_set_sensitive(glfer.qso_menu_item, FALSE);
    gtk_widget_set_sensitive(glfer.test_menu_item, FALSE);
  } else {
    /* show_message("Device%s opened \n", opt.ctrl_device); */
    /* control device initialisation ok, enable transmission functions */
    glfer.init_done = TRUE;
    gtk_widget_set_sensitive(glfer.qso_menu_item, TRUE);
    gtk_widget_set_sensitive(glfer.test_menu_item, TRUE);
    if ((action != NULL) && (!strcmp(action, "save")))
      rc_file_write();
  }
}


static void port_prefs_to_widgets(void)
{
  gtk_entry_set_text(GTK_ENTRY(control_port_en), opt.ctrl_device);
  if (opt.device_type == DEV_SERIAL) {
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(serial_bu), TRUE);
  } else if (opt.device_type == DEV_PARALLEL) {
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(parallel_bu), TRUE);
  } else {
    g_assert_not_reached();
  }
}


static void port_widgets_to_prefs(GtkWidget * widget, gpointer data)
{
  GtkWidget *dialog, *ok_bu, *cancel_bu, *label;
  char *selected_device = NULL;

  selected_device = gtk_editable_get_chars(GTK_EDITABLE(control_port_en), 0, -1);

  if ((glfer.init_done == FALSE) || strcmp(opt.ctrl_device, selected_device)) {
    /* first initialisation or different device selected */
    dialog = gtk_dialog_new();
    g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK(gtk_widget_destroyed), &dialog);

    /* setup window properties */
    gtk_window_set_wmclass(GTK_WINDOW(dialog), "warning", "glfer");
    /* set dialog title */
    gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
    gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    /* make the dialog modal */
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    /* not user resizable */
    gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
    /* set border width */
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);

    label = gtk_label_new("Selected control port may become unstable during initialisation!\nPlease make sure that the TX is off.");
    gtk_misc_set_padding(GTK_MISC(label), 20, 20);
    /* OK button */
    ok_bu = gtk_button_new_with_label("Ok");

    g_signal_connect(G_OBJECT(ok_bu), "clicked", G_CALLBACK(port_change_device), data);
    g_signal_connect_swapped(G_OBJECT(ok_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(port_window));
    g_signal_connect_swapped(G_OBJECT(ok_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(dialog));

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), ok_bu);
    /* cancel button */
    cancel_bu = gtk_button_new_with_label("Cancel");

    g_signal_connect_swapped(G_OBJECT(ok_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(port_window));
    g_signal_connect_swapped(G_OBJECT(cancel_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(dialog));
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), cancel_bu);

    /* Add the label, and show everything we've added to the dialog. */
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
    gtk_widget_show_all(dialog);
  }
}


void port_settings_dialog(GtkWidget * widget, gpointer data)
{
  GtkWidget *temp_bu;
  GtkWidget *tmp_vb;
  GtkWidget *pa_fr;
  GtkWidget *tmp_hb;
  GList *serial_dev_it = NULL, *parallel_dev_it = NULL;
  GSList *devtype_gr;

  if (port_window && port_window->window) {
    gtk_widget_map(port_window);
    gdk_window_raise(port_window->window);
    return;
  }
  port_window = gtk_dialog_new();
  g_signal_connect(G_OBJECT(port_window), "destroy", G_CALLBACK(gtk_widget_destroyed), &port_window);

  /* setup window properties */
  gtk_window_set_wmclass(GTK_WINDOW(port_window), "portprefs", "glfer");
  gtk_window_set_title(GTK_WINDOW(port_window), "Device preferences");
  gtk_window_set_policy(GTK_WINDOW(port_window), FALSE, FALSE, FALSE);
  gtk_window_position(GTK_WINDOW(port_window), GTK_WIN_POS_NONE);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(port_window)->vbox), 5);

  tmp_hb = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(port_window)->vbox), tmp_hb, FALSE, FALSE, 10);
  gtk_widget_show(tmp_hb);

  /* control devices frame */
  pa_fr = gtk_frame_new("Control device");
  gtk_box_pack_start(GTK_BOX(tmp_hb), pa_fr, FALSE, FALSE, 10);
  gtk_widget_show(pa_fr);
  /* control devices vbox */
  tmp_vb = gtk_vbox_new(FALSE, 5);
  gtk_container_add(GTK_CONTAINER(pa_fr), tmp_vb);
  gtk_widget_show(tmp_vb);

  /* control port entry */
  compound_entry("/dev/", &control_port_en, tmp_vb);
  gtk_tooltips_set_tip(glfer.tt, control_port_en, "Control port name (e.g. ttyS0 for serial, lp0 for parallel)", NULL);

  /* device type radio button */
  serial_bu = gtk_radio_button_new_with_label(NULL, "Serial port");
  gtk_tooltips_set_tip(glfer.tt, serial_bu, "Enable serial control port", NULL);
  devtype_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(serial_bu));
  gtk_box_pack_start(GTK_BOX(tmp_vb), serial_bu, TRUE, TRUE, 5);
  gtk_widget_show(serial_bu);

  /* device type radio button */
  parallel_bu = gtk_radio_button_new_with_label(devtype_gr, "Parallel port");
  gtk_tooltips_set_tip(glfer.tt, parallel_bu, "Enable parallel control port", NULL);
  devtype_gr = gtk_radio_button_group(GTK_RADIO_BUTTON(parallel_bu));
  gtk_box_pack_start(GTK_BOX(tmp_vb), parallel_bu, TRUE, TRUE, 5);
  gtk_widget_show(parallel_bu);

  /* buttons */
  temp_bu = gtk_button_new_with_label("Ok");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Accept settings and close window", NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(port_widgets_to_prefs), NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(port_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  temp_bu = gtk_button_new_with_label("Save");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Save settings", NULL);
  //g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(save_event), NULL);
  g_signal_connect(G_OBJECT(temp_bu), "clicked", G_CALLBACK(port_widgets_to_prefs), "save");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(port_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  temp_bu = gtk_button_new_with_label("Cancel");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Close window", NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(port_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(port_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* load current values */
  port_prefs_to_widgets();

  gtk_widget_show(port_window);
}


static void audio_prefs_to_widgets(void)
{
  gchar *tmp_str;

  tmp_str = g_strdup_printf("%s", opt.audio_device);
  gtk_entry_set_text(GTK_ENTRY(audio_device_en), tmp_str);
  g_free(tmp_str);
}

static void audio_widgets_to_prefs(GtkWidget * widget, gpointer data)
{
  gchar *tmp_str;
  
  tmp_str = gtk_editable_get_chars(GTK_EDITABLE(audio_device_en), 0, -1);
  strncpy(opt.audio_device, tmp_str, 9);
  g_free(tmp_str);
  close_audio(); /* close current audio device */
  init_audio(NULL); /* open new audio device */
}

void audio_settings_dialog(GtkWidget * widget, gpointer data)
{
  GtkWidget *temp_bu;
  GtkWidget *dev_fr;
  GtkWidget *tmp_hb;

  if (audio_window && audio_window->window) {
    gtk_widget_map(audio_window);
    gdk_window_raise(audio_window->window);
    return;
  }
  audio_window = gtk_dialog_new();
  g_signal_connect(G_OBJECT(audio_window), "destroy", G_CALLBACK(gtk_widget_destroyed), &audio_window);

  /* setup window properties */
  gtk_window_set_wmclass(GTK_WINDOW(audio_window), "audioprefs", "glfer");
  gtk_window_set_title(GTK_WINDOW(audio_window), "Audio settings");
  gtk_window_set_policy(GTK_WINDOW(audio_window), FALSE, FALSE, FALSE);
  gtk_window_position(GTK_WINDOW(audio_window), GTK_WIN_POS_NONE);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(audio_window)->vbox), 5);

  /* Device frame */
  dev_fr = gtk_frame_new("Device");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(audio_window)->vbox), dev_fr, FALSE, FALSE, 10);
  gtk_widget_show(dev_fr);

  /* Device hbox */
  tmp_hb = gtk_hbox_new(FALSE, 5);
  gtk_container_add(GTK_CONTAINER(dev_fr), tmp_hb);
  gtk_widget_show(tmp_hb);

  compound_entry("Path :", &audio_device_en, tmp_hb);
  gtk_tooltips_set_tip(glfer.tt, audio_device_en, "Audio device path like /dev/dsp or /dev/dsp1", NULL);

  /* OK button */
  temp_bu = gtk_button_new_with_label("Ok");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Accept changes and close window", NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(audio_widgets_to_prefs), GTK_OBJECT(audio_window));
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(audio_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(audio_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* Cancel button */
  temp_bu = gtk_button_new_with_label("Cancel");
  gtk_tooltips_set_tip(glfer.tt, temp_bu, "Discard changes and close window", NULL);
  g_signal_connect_swapped(G_OBJECT(temp_bu), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(audio_window));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(audio_window)->action_area), temp_bu, TRUE, TRUE, 0);
  gtk_widget_show(temp_bu);

  /* load current values */
  audio_prefs_to_widgets();

  gtk_widget_show(audio_window);
}
