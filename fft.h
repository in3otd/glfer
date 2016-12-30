/* fft.h
 * 
 * Copyright (C) 2001-2008 Claudio Girardi
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

#ifndef _FFT_H_
#define _FFT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBRFFTW
#include <rfftw.h>
#else
#include "fft_radix2.h"
#endif /* HAVE_LIBRFFTW */


#ifdef HAVE_LIBRFFTW

typedef struct {
  fftw_plan plan;
  fftw_real *inbuf_audio;
  fftw_real *inbuf_fft;
  fftw_real *outbuf;
  int n;                        /* FFT size */
  float *window;                /* FFT window data */
  int window_type;              /* FFT window type */
  float overlap;		/* percentage of overlap between FFT blocks */
  float a;                      /* RA9MB nonlinear processing parameter */
  int limiter;
  int sub_mean;
} fft_params_t;

#else

typedef struct {
  float *inbuf_audio;
  float *inbuf_fft;
  float *outbuf;
  int n;
  float *window;
  int window_type;
  float overlap;
  float a;                      /* RA9MB nonlinear processing parameter */
  int limiter;
  int sub_mean;
} fft_params_t;

#endif /* HAVE_LIBRFFTW */

enum {HANNING_WINDOW = 0, BLACKMAN_WINDOW, GAUSSIAN_WINDOW, WELCH_WINDOW, BARTLETT_WINDOW, RECTANGULAR_WINDOW, HAMMING_WINDOW, KAISER_WINDOW};

typedef struct _fft_window_t fft_window_t;

struct _fft_window_t
{
  char *name;
  int type;
};

void prepare_audio(float *audio_buf, fft_params_t * params);
void fft_init(fft_params_t * params);
void fft_do(float *audio_buf, fft_params_t * params);
void fft_psd(float *psd_buf, float *phase_buf, fft_params_t * params);
void fft_close(fft_params_t * params);

void compute_floor(float *psd_buf, int n, float *sig_pwr_p, float *floor_pwr_p , float *peak_pwr_p, unsigned int *peak_bin_p);

void goertzel(float *audio_buf, float *psd_buf, float *phase_buf, fft_params_t * params);

#endif /* #ifndef _FFT_H_ */
