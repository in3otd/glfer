/* cw_rx.h
 * 
 * Copyright (C) 2007 Claudio Girardi
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
#include <math.h>
#include "glfer.h"
#include "cw_rx.h"


extern opt_t opt;
extern glfer_t glfer;

float *bb_buf_re = NULL; /* buffer for holding baseband data */
float *bb_buf_im = NULL; /* buffer for holding baseband data */

float wpm = 12;           /* morse code speed in words per minute */

#define DOWN 8                           /* the downsampling factor */
#define SAMP2 (opt.sample_rate / DOWN)   /* sample rate after downsampling, in samples/second */
#define DOTSEC (1.2 / wpm)               /* duration of one dot in seconds */
#define DOTLEN (SAMP2 * 1.2/ wpm)        /* duration of one dot in samples (after downsampling) */


void rx_cw_init(int n_samples)
{
  /* allocate memory for the baseband buffer */
  bb_buf_re = malloc(n_samples * sizeof(float));
  if (!bb_buf_re) {
    fprintf(stderr, "error allocating bb_buf_re\n");
    exit(-1);
  }

  bb_buf_im = malloc(n_samples * sizeof(float));
  if (!bb_buf_im) {
    fprintf(stderr, "error allocating bb_buf_im\n");
    exit(-1);
  }
}


void rx_cw(float *audio_buf, int n_samples)
{
  float delta;
  float frequency = 800; /* CW RX fixed at 800 Hz */
  float phaseacc;
  float z_re, z_im;
  int i;

  /* compute phase increment expected at our specific rx tone freq */
  delta = 2.0 * M_PI * frequency / opt.sample_rate;;
  
  for (i = 0; i < n_samples; i++) {
    /* Mix with the internal NCO to convert do DC */
    bb_buf_re[i] = audio_buf[i] * cos(phaseacc);
    bb_buf_im[i] = audio_buf[i] * sin(phaseacc);
    phaseacc += delta;
    
    if (phaseacc > M_PI)
      phaseacc -= 2.0 * M_PI;
  }

  

}
