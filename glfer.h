/* glfer.h
 * 
 * Copyright (C) 2001-2010 Claudio Girardi
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

#ifndef _GLFER_H_
#define _GLFER_H_

#include <gtk/gtk.h>

#define RELDATE "29 Jan 2010"

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#define	QRSS 1
#define	DFCW 2

#define	DEV_SERIAL 0
#define	DEV_PARALLEL 1

enum { SCALE_LIN, SCALE_LIN_MAX0, SCALE_LOG, SCALE_LOG_MAX0 };

enum {MODE_NONE = -1, MODE_FFT, MODE_MTM, MODE_HPARMA, MODE_LMP};

enum {HSV, THRESH, COOL, HOT, BW, BONE, COPPER, OTD, PALETTES};

typedef enum 
  {SOURCE_NONE, SOUNDCARD_SOURCE, FILE_SOURCE}
datasource_t;


typedef enum
  { NO_AVG, AVG_SUMAVG, AVG_PLAIN, AVG_SUMEXTREME}
avgmode_t;

/* function prototypes */
void show_message(gchar * fmt, ...);


typedef struct {
  char *program_name;		/* the name used for invoking the program */
  int mode;			/* current mode */
  int scale_type;		/* display scale type */

  /* input data processing */
  int data_block_size;          /* number of samples used for the FFT */
  float data_blocks_overlap;	/* overlap between FFT blocks */
  float display_update_time;	/* display update time */
  float limiter_a;              /* RA9MB nonlinear processing parameter */
  int enable_limiter;           /* enable limter */

  /* MTM parameters */
  float mtm_w;			/* mtm window relative bandwidth (0 to 0.5) */
  int mtm_k;			/* number of windows to use */

  /* HP-ARMA parameters */
  int hparma_t;			/* number of equations */
  int hparma_p_e;		/* number of poles */

  /* LMP parameters */
  int lmp_av;			/* number of data blocks for the internal average */

  /* FFT parameters */
  int window_type;		/* FFT window */

  /* input device parameters */
  char *audio_device;		/* audio device name */
  int sample_rate;		/* sound card sample rate */

  /* CW/QRSS TX parameters */
  float dot_time;		/* dot duration */
  float dfcw_gap_time;		/* gap between dots duration in DFCW mode */
  int tx_mode;			/* transmission mode: QRSS or DFCW */
  float dash_dot_ratio;		/* dash to dot ratio in QRSS */
  float ptt_delay;		/* delay in key down after closing PTT */
  float sidetone_freq;		/* sidetone frequency */
  int sidetone;			/* optional sidetone enabled ? */
  float dfcw_dot_freq;		/* DFCW dot tone frequency */
  float dfcw_dash_freq;		/* DFCW dash tone frequency */
  int beacon_mode;		/* beacon mode ON or OFF */
  float beacon_pause;           /* beacon mode pause between messages */
  int beacon_tx_pause;          /* beacon on or off during pauses */
  char *ctrl_device;		/* device controlling KEY and PTT */
  int device_type;		/* control device type */

  /* spectrogram parameter */
  float offset_freq;		/* frequency offset for spectrogram frequency scale */
  float thr_level;		/* spectrogram display threshold level */
  int autoscale;                /* enable autoscale mode (otherwise use min and max settings below) */
  float max_level_db;           /* maximum level to scale display i.e. for 255 (in dB) */
  float min_level_db;           /* minimum level to scale display i.e. for 0 (in dB) */
  avgmode_t averaging;          /* averaging mode */
  int  avgsamples;              /* number of samples over which to average (moving average) */
  float min_avgband;            /* lower frequency cutoff for extreme averaging */
  float max_avgband;            /* higher frequency cutoff for extreme averaging */
  int palette;                  /* palette type */
}
opt_t;


typedef struct {
  GtkTooltips *tt;		/* tooltips */
  GtkWidget *qso_menu_item;
  GtkWidget *test_menu_item;
  int init_done;		/* start-up inizialization done */
  int first_buffer;             /* first data buffer acquired from source */
  datasource_t input_source;    /* input data source */
  float cpu_usage;		/* CPU usage */
  int current_mode;		/* current mode */
  GtkWidget *scope_window;
  float avgmax;                 /* average psd over spectrum */
  double avgvar;                /* variance above average psd over spectrum */
  int avgfill;                  /* number of samples in averaging buffer */
  float peakfreq;               /* peak frequency */
  float peakval;                /* peak value */
  float avgtime;                /* averaging time */
} glfer_t;


typedef struct {
  double start;
  double stop;
  double delta;
} perf_timer_t;


#endif /* #ifndef _GLFER_H_ */
