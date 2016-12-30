/* source.c

 * Copyright (C) 2002-2008 Claudio Girardi
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "source.h"
#include "audio.h"
#include "fft.h"
#include "mtm.h"
#include "hparma.h"
#include "lmp.h"
#include "cw_rx.h"
#include "util.h"
#include "avg.h"
#include "glfer.h"
#include "wav_fmt.h"
#include "g_main.h"
#include "g_scope.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif


static int audio_fd = -1;
static int psd_n;
static char *fname = NULL;
static float *psdbuf = NULL;
static gint input_tag = -1;
static int file_ended = 0;

perf_timer_t perf_timer, proc_timer;

extern opt_t opt;
extern glfer_t glfer;
extern avg_data_t avgdata;
extern fft_params_t fft_gram_par;
extern mtm_params_t mtm_gram_par;
extern hparma_params_t hparma_gram_par;
extern lmp_params_t lmp_gram_par;

static float getclock(void)
{

  clock_t nclock;
  float nsec;

  nclock = clock();
  nsec = (float) nclock / CLOCKS_PER_SEC;

  return nsec;
}


static float timefn(int action)
{
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv;
  double newtime;
  static double oldtime = 0.0;

  if (gettimeofday(&tv, NULL))
    perror("gettimeofday");
  newtime = (double) tv.tv_sec + (double) tv.tv_usec / 1.0e06;

  //printf("%e %e %e\n", newtime, oldtime, newtime - oldtime);

  if (action == 0)
    oldtime = newtime;

  return (newtime - oldtime);
#endif /* HAVE_GETTIMEOFDAY */
}


/* reads the audio data, calls the fft processing function and the drawing
 * function
 * (called from within gtk_main whenever data is available */
static void audio_available(gpointer data, gint source, GdkInputCondition condition)
{
  float *audio_buf;
  int i, n;
  int n_eff = opt.data_block_size * (1.0 - opt.data_blocks_overlap);

  if (fname) { /* we have a file open for reading */
      wav_read(&audio_buf, &n);
    if (n == 0) {
      file_ended = 1;
      stop_reading_audio();
      close_wav_file();
    }
  } else {
    /* fill audio_buf */
    audio_read(&audio_buf, &n);
    /* n is the number of blocks of requested size read from the audio device */ }

  for (i = 0; i < n; i++) {
    /* get current time */
    perf_timer.stop = tc_time();
    /* elapsed time since last call */
    perf_timer.delta = perf_timer.stop - perf_timer.start;
    glfer.cpu_usage = proc_timer.delta / perf_timer.delta;
    /* start new period */
    perf_timer.start = perf_timer.stop;
    /* start new data processing period */
    proc_timer.start = perf_timer.stop;

    switch (glfer.current_mode) {
    case MODE_FFT :
      fft_do(audio_buf + i * n_eff, &fft_gram_par);
      fft_psd(psdbuf, NULL, &fft_gram_par);
      if (glfer.scope_window) scope_window_draw(fft_gram_par);
      break;
    case MODE_MTM :
      mtm_do(audio_buf + i * n_eff, psdbuf, NULL, &mtm_gram_par);
      if (glfer.scope_window) scope_window_draw(mtm_gram_par.fft);
      break;
    case MODE_HPARMA :
      hparma_do(audio_buf + i * n_eff, psdbuf, NULL, &hparma_gram_par);
      if (glfer.scope_window) scope_window_draw(hparma_gram_par.fft);
      break;
    case MODE_LMP :
      lmp_do(audio_buf + i * n_eff, psdbuf, NULL, &lmp_gram_par);
      if (glfer.scope_window) scope_window_draw(lmp_gram_par.fft);
      break;
    default:
      g_print("audio_available: MODE_UNKNOWN\n");
    }

    /* jason_do(audio_buf + i * opt.data_block_size, opt.data_block_size); */
    //rx_cw(audio_buf + i * opt.data_block_size, opt.data_block_size);
    main_window_draw(psdbuf);

    /* end of data processing */
    proc_timer.stop = tc_time();
    proc_timer.delta = proc_timer.stop - proc_timer.start;
  }
}


void init_audio(char *filename)
{
  int n_eff = opt.data_block_size * (1.0 - opt.data_blocks_overlap);

  if (filename) {
    /* use a new file as audio source */
    /* in case there is a file currently open as audio source, close it */
    if (glfer.input_source == FILE_SOURCE) {
         close_wav_file();
	 /* free filename string */
	 free(fname);
	 /* set pointer to null (to signal that source is not a file anymore) */
	 fname = NULL;
    } else if (glfer.input_source == SOUNDCARD_SOURCE) {
      /* close soundcard */
      audio_close();
    }    
    /* copy file name, as passed parameter might be deallocated later */
    fname = strdup(filename);
    audio_fd = open_wav_file(fname, n_eff, &opt.sample_rate);
    file_ended = 0;
    glfer.input_source = FILE_SOURCE;
  } else /* no filename specified, continue use current audio source */
    if (glfer.input_source == SOUNDCARD_SOURCE) { 
      glfer.input_source = SOUNDCARD_SOURCE; /* not needed... */
    } else if (glfer.input_source == FILE_SOURCE) {  /* continue to read current file */
      glfer.input_source = FILE_SOURCE; /* not needed... */
    } else { /* no current audio source and no filename specified; read from soundcard */
      audio_fd = audio_init(opt.audio_device, &opt.sample_rate, n_eff);
      glfer.input_source = SOUNDCARD_SOURCE;
    }
}


void close_audio(void)
{
  /* close current source */
  if (glfer.input_source == SOUNDCARD_SOURCE) {
    /* was reading from soundcard */
    audio_close();
  } else {
    /* was reading from file */
    close_wav_file();
    /* free filename string */
    free(fname);
    /* set pointer to null (to signal that source is not a file anymore) */
    fname = NULL;
  }
  audio_fd = -1;
  glfer.input_source = SOURCE_NONE;
}


void toggle_stop_start(GtkWidget * widget, gpointer data)
{
  if (input_tag == -1) {
    start_reading_audio();
  } else {
    stop_reading_audio();
  }
}


void start_reading_audio()
{
  int n_eff = opt.data_block_size * (1.0 - opt.data_blocks_overlap);

  if (glfer.input_source == SOUNDCARD_SOURCE) { /* read from soundcard */
    /* audio_available() has to be called whenever there's data on audio_fd */
    input_tag = gdk_input_add(audio_fd, GDK_INPUT_READ, audio_available, NULL);
  } else if (glfer.input_source == FILE_SOURCE) { /* read from file */
    if (file_ended == 1) {
      audio_fd = open_wav_file(fname, n_eff, &opt.sample_rate);
      file_ended = 0;
    }
    input_tag = g_idle_add((GSourceFunc)audio_available, NULL);
  }
  stop_start_button_set_label("Stop");
}


void stop_reading_audio()
{
  if (input_tag != -1) {
    /* if currently reading from a source */
    gtk_input_remove(input_tag);
    input_tag = -1;
    stop_start_button_set_label("Start");
    /* do not close an open file, so that pressing again the Start/Stop button continues the file processing */
  }
}


void change_params(int mode_sel)
{
  /* save current open file name (if any) */
  char *current_fname;
  
  if (fname)
    current_fname = strdup(fname);
  else
    current_fname = NULL;

  /* check if this is the first call at program start-up */
  if (glfer.current_mode != MODE_NONE) {
    /* stop reading from the current data source */
    stop_reading_audio();
    /* close current mode */
    switch (glfer.current_mode) {
    case MODE_FFT:
      fft_close(&fft_gram_par);
      break;
    case MODE_MTM:
      mtm_close(&mtm_gram_par);
      break;
    case MODE_HPARMA:
      hparma_close(&hparma_gram_par);
      break;
    case MODE_LMP:
      lmp_close(&lmp_gram_par);
      break;
    default:
      g_print("change_params: MODE_UNKNOWN\n");
    }

    FREE_MAYBE(psdbuf);
  }

  /* initialize mode */
  switch (mode_sel) {
  case MODE_FFT :
    if ((fft_gram_par.n != opt.data_block_size) || (fft_gram_par.overlap != opt.data_blocks_overlap)) {
      close_audio();
      init_audio(current_fname);
    }
    if (fft_gram_par.n != opt.data_block_size) {
      /* delete and reallocate averaging buffer */
      delete_avg(&avgdata);
      alloc_avg(&avgdata, opt.data_block_size, opt.avgsamples);
      glfer.avgfill = 0; /* reset ratio of average buffer full */
      glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-opt.data_blocks_overlap);
    }

    psd_n = opt.data_block_size / 2 + 1;
    psdbuf = calloc(psd_n, sizeof(float));
    
    fft_gram_par.n = opt.data_block_size;
    fft_gram_par.window_type = opt.window_type;
    fft_gram_par.overlap = opt.data_blocks_overlap;
    fft_gram_par.a = opt.limiter_a;
    fft_gram_par.limiter = opt.enable_limiter;
    fft_init(&fft_gram_par);
    break;
  case MODE_MTM :
    if ((mtm_gram_par.fft.n != opt.data_block_size) || (mtm_gram_par.fft.overlap != opt.data_blocks_overlap)) {
      close_audio();
      init_audio(current_fname);
    }
    if (fft_gram_par.n != opt.data_block_size) {
      /* delete and reallocate averaging buffer */
      delete_avg(&avgdata);
      alloc_avg(&avgdata, opt.data_block_size, opt.avgsamples);
      glfer.avgfill = 0; /* reset ratio of average buffer full */
      glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-opt.data_blocks_overlap);
    }
    
    psd_n = opt.data_block_size / 2 + 1;
    psdbuf = calloc(psd_n, sizeof(float));

    mtm_gram_par.fft.n = opt.data_block_size;;
    mtm_gram_par.fft.window_type = RECTANGULAR_WINDOW; /* that is, no windowing is used */
    mtm_gram_par.fft.overlap = opt.data_blocks_overlap;
    mtm_gram_par.fft.a = opt.limiter_a;
    mtm_gram_par.fft.limiter = opt.enable_limiter;
    mtm_gram_par.w = opt.mtm_w;
    mtm_gram_par.kmax = opt.mtm_k;
    mtm_init(&mtm_gram_par);
    break;
  case MODE_HPARMA :
    if ((hparma_gram_par.fft.n != opt.data_block_size) || (hparma_gram_par.fft.overlap != opt.data_blocks_overlap)) {
      close_audio();
      init_audio(current_fname);
    }
    if (fft_gram_par.n != opt.data_block_size) {
      /* delete and reallocate averaging buffer */
      delete_avg(&avgdata);
      alloc_avg(&avgdata, opt.data_block_size, opt.avgsamples);
      glfer.avgfill = 0; /* reset ratio of average buffer full */
      glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-opt.data_blocks_overlap);
    }

    psd_n = opt.data_block_size / 2 + 1;
    psdbuf = calloc(psd_n, sizeof(float));

    hparma_gram_par.fft.n = opt.data_block_size;
    hparma_gram_par.fft.window_type = RECTANGULAR_WINDOW;
    hparma_gram_par.fft.overlap = opt.data_blocks_overlap;
    hparma_gram_par.fft.a = opt.limiter_a;
    hparma_gram_par.fft.limiter = opt.enable_limiter;
    hparma_gram_par.t = opt.hparma_t;
    hparma_gram_par.p_e = opt.hparma_p_e;
    hparma_gram_par.q_e = -1;
    hparma_init(&hparma_gram_par);
    break;
  case MODE_LMP :
    if ((lmp_gram_par.fft.n != opt.data_block_size) || (lmp_gram_par.fft.overlap != opt.data_blocks_overlap)) {
      close_audio();
      init_audio(current_fname);
    }
    if (fft_gram_par.n != opt.data_block_size) {
      /* delete and reallocate averaging buffer */
      delete_avg(&avgdata);
      alloc_avg(&avgdata, opt.data_block_size, opt.avgsamples);
      glfer.avgfill = 0; /* reset ratio of average buffer full */
      glfer.avgtime = (float) opt.avgsamples * (float) opt.data_block_size * (1.0 / (float) opt.sample_rate)*(1.0-opt.data_blocks_overlap); 
   }

    psd_n = opt.data_block_size / 2 + 1;
    psdbuf = calloc(psd_n, sizeof(float));

    lmp_gram_par.fft.n = opt.data_block_size;
    lmp_gram_par.fft.window_type = RECTANGULAR_WINDOW;
    lmp_gram_par.fft.overlap = opt.data_blocks_overlap;
    lmp_gram_par.fft.a = opt.limiter_a;
    lmp_gram_par.fft.limiter = opt.enable_limiter;
    lmp_gram_par.avg = opt.lmp_av;
    lmp_init(&lmp_gram_par);
    break;
  default:
    g_print("change_params: MODE_UNKNOWN\n");
  }

  glfer.current_mode = mode_sel;

  main_window_init(psd_n, 0, (float) opt.sample_rate / 2);

  start_reading_audio();
}
