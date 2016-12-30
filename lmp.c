/* lmp.c

 * Copyright (C) 2004 Claudio Girardi
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBRFFTW
#include <rfftw.h>
#else
#include "fft_radix2.h"
#endif /* HAVE_LIBRFFTW */


#include "glfer.h"
#include "util.h"
#include "lmp.h"
#include "fft.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif

extern opt_t opt;

static float **psdbufl = NULL;
static double *my = NULL;
static double *sy = NULL;

#ifdef HAVE_LIBRFFTW
static fftw_real *mu = NULL;
#else
static float *mu = NULL;
#endif /* HAVE_LIBRFFTW */

int nl;

void lmp_init(lmp_params_t * params)
{
  int i, j;
  int n = params->fft.n;
  int kmax = params->kmax;
  double **v = params->window;

#ifdef HAVE_LIBRFFTW
  params->fft.plan = rfftw_create_plan(n, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);

  params->fft.inbuf_audio = calloc(n, sizeof(fftw_real));
  params->fft.inbuf_fft = calloc(n, sizeof(fftw_real));
  params->fft.outbuf = calloc(n, sizeof(fftw_real));

  mu = calloc(n, sizeof(fftw_real));
#else
  params->fft.inbuf_audio = calloc(n, sizeof(float));
  params->fft.inbuf_fft = calloc(n, sizeof(float));
  params->fft.outbuf = params->fft.inbuf_fft;

  mu = calloc(n, sizeof(float));
#endif /* HAVE_LIBRFFTW */

  params->fft.sub_mean = opt.autoscale;  // sub_mean only if autoscale (?)

  nl = params->avg;

  /* spectra of the subsequences */
  psdbufl = matrix(0, nl - 1, 0, n - 1);
  /* clear psdbufl */
  for (j = 0; j < nl; j++) {
    for (i = 0; i < n / 2 + 1; i++) {
      psdbufl[j][i] = 0.0;
    }
  }

  /* mean */
  my = calloc(n, sizeof(double));
  /* variance */
  sy = calloc(n, sizeof(double));
}


void lmp_do(float *audio_buf, float *psd_buf, float *phase_buf, lmp_params_t * params)
{
  int i, j;
  static int j_l = 0;
  int n_fft = params->fft.n;
  double v_hat;

  /* apply overlap, limting, windowing, etc. according to active options */
  prepare_audio(audio_buf, &params->fft);

  /* compute the FFT for the subsequences */
  //  for (j = 0; j < nll; j++) {
    for (i = 0; i < n_fft; i++) {
      params->fft.inbuf_fft[i] = params->fft.inbuf_audio[i];
    }
    /* compute the fft */
#ifdef HAVE_LIBRFFTW
    rfftw_one(params->fft.plan, params->fft.inbuf_fft, params->fft.outbuf);
#else
    fft_real_radix2_transform(params->fft.inbuf_fft, n_fft);
#endif /* HAVE_LIBRFFTW */

    /* compute the power spectrum of the subsequence */
    fft_psd(psdbufl[j_l], NULL, &params->fft);
    //  }
    /*
    for (i = 0; i < n_fft / 2 + 1; i++) {
      psdbufl[j_l][i] = rand() / (RAND_MAX + 1.0);
      if (i == 100) psdbufl[j_l][i] += 0.5;
    }
    */
  /* compute my */
    for (i = 0; i < n_fft / 2 + 1; i++) {
      my[i] = 0.0;
      for (j = 0; j < nl; j++) {
	my[i] += psdbufl[j][i];
      }
      my[i] /= nl;
    }
  
  /* compute sy */
  for (i = 0; i < n_fft / 2 + 1; i++) {
    sy[i] = 0.0;
    for (j = 0; j < nl; j++) {
      sy[i] += (psdbufl[j][i] - my[i]) * (psdbufl[j][i] - my[i]);
    }
    sy[i] /= (nl - 1);
  }

  for (i = 0; i < n_fft / 2 + 1; i++) {
    //v_hat = 0.5 * (my[i] - sqrt(fabs(my[i] * my[i] - sy[i])));
    v_hat = my[i] * my[i] - sy[i];
    if (v_hat < 0.0) v_hat = 0.0;
    v_hat = 0.5 * (my[i] - sqrt(v_hat));

    psd_buf[i] = -sqrt(nl / 2.0) + (nl * my[i]) / (2.0 * sqrt(2.0 * nl) * v_hat);
    if (psd_buf[i] <= 1.0e-3 ) psd_buf[i] = 1e-3;
  }
  psd_buf[0] = 1e-3;

  for (i = 0; i < n_fft / 2 + 1; i++) {
    //fprintf(stderr, "%i %e\t", i, psd_buf[i]);
  } 

  /*
  for (i = 0; i < n_fft / 2 + 1; i++) {
    psd_buf[i] = 0.0;
    for (j = 0; j < nl; j++) {
      if (psdbufl[j][i] < 1e-13)psdbufl[j][i] = 1e-13;
      psd_buf[i] += 1.0 / psdbufl[j][i];
    }
    psd_buf[i] /= nl;
    psd_buf[i] = 1.0 / psd_buf[i];
  }
  */

  j_l++;
  if (j_l == nl)
    j_l = 0;

}


void lmp_close(lmp_params_t * params)
{
  int n = params->fft.n;

  FREE_MAYBE(params->fft.inbuf_audio);
  FREE_MAYBE(params->fft.inbuf_fft);
#ifdef HAVE_LIBRFFTW
  rfftw_destroy_plan(params->fft.plan);
  FREE_MAYBE(params->fft.outbuf);
#endif /* HAVE_LIBRFFTW */
}
