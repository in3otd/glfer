/* mtm.c

 * Copyright (C) 2002 Claudio Girardi
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
#include "mtm.h"
#include "fft.h"
#include "bell-p-w.h"
#include "g-l_dpss.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif

extern opt_t opt;

static float *psdbuftmp = NULL;
static float *hn = NULL;

#ifdef HAVE_LIBRFFTW
static fftw_real *mu = NULL;
#else
static float *mu = NULL;
#endif /* HAVE_LIBRFFTW */

static float *ftest = NULL;

static double *U0 = NULL;

static float sum_U0_sqr;


static void compute_dspwf(mtm_params_t * params)
{
  int i, j, ifault, totit;
  int n = params->fft.n;
  int kmax = params->kmax;
  double w = params->w;
  double **v = params->window;
  double *sig = params->sig;

//  ifault = dpss(n, kmax, n, w, v, sig, &totit);
  ifault = gl_dpss(n, kmax, n, w, v, sig, &totit);

  if (ifault != 0)
    fprintf(stderr, "Error: ifault = %i\n", ifault);

  for (j = 0; j <= kmax; j++) {
    U0[j] = 0.0;
    for (i = 1; i <= n; i++) {
      U0[j] += v[i][j];
    }
    D(fprintf(stderr, "sig[%i] = %e\n", j, sig[j]));
  }
}


void mtm_init(mtm_params_t * params)
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

  psdbuftmp = calloc(n, sizeof(float));
  hn = calloc(n, sizeof(float));
  ftest = calloc(n, sizeof(float));

  /* allocate space for the dspwf */
  params->window = dmatrix(1, n, 0, kmax);
  params->sig = dvector(0, kmax);
  U0 = dvector(0, kmax);

  compute_dspwf(params);

  v = params->window;
  sum_U0_sqr = 0.0;
  for (j = 0; j <= kmax; j++) {
    sum_U0_sqr += U0[j] * U0[j];
  }

  for (i = 0; i < n; i++) {
    hn[i] = 0.0;
    for (j = 0; j <= kmax; j++) {
      hn[i] += U0[j] * v[i + 1][j];
    }
    hn[i] /= sum_U0_sqr;
  }

#ifdef FIXME
  for (j = 0; j <= kmax; j++) {
    FILE *fff;
    char fffname[10];

    sprintf(fffname, "v%i", j);
    fff = fopen(fffname, "w");
    for (i = 0; i < n; i++) {
      fprintf(fff, "%e\n", v[i + 1][j]);
    }
    fclose(fff);
  }
#endif
}


void mtm_do(float *audio_buf, float *psd_buf, float *phase_buf, mtm_params_t * params)
{
  int i, j;
  int n_fft = params->fft.n;
  int k = params->kmax;
  double tmpr, tmpi, num_ftest;

  /* apply overlap, limting, windowing, etc. according to active options */
  prepare_audio(audio_buf, &params->fft);

  /* compute mu(f) from the data sequence */
  for (i = 0; i < n_fft; i++) {
    /* apply window hn to the sequence */
    params->fft.inbuf_fft[i] = params->fft.inbuf_audio[i] * hn[i];
  }
  /* compute mu(f) with a fft */
#ifdef HAVE_LIBRFFTW
  rfftw_one(params->fft.plan, params->fft.inbuf_fft, mu);
#else
  fft_real_radix2_transform(params->fft.inbuf_fft, n_fft);
#endif /* HAVE_LIBRFFTW */

  //fft_psd(psdbuf, NULL, &params->fft);
  //draw_spect(n_fft / 2, psdbuf, "mu(f)", PLOT_LOG, 470);

  for (i = 0; i < (n_fft + 1) / 2; i++) {
    psd_buf[i] = 0.0;
    ftest[i] = 0.0;
  }
  if (n_fft % 2 == 0) {
    psd_buf[n_fft / 2] = 0.0;
    ftest[n_fft / 2] = 0.0;
  }


  for (j = 0; j <= k; j++) {
    for (i = 0; i < n_fft; i++) {
      params->fft.inbuf_fft[i] = params->window[i + 1][j] * params->fft.inbuf_audio[i];
    }

    /* compute the fft */
#ifdef HAVE_LIBRFFTW
    rfftw_one(params->fft.plan, params->fft.inbuf_fft, params->fft.outbuf);
#else
    fft_real_radix2_transform(params->fft.inbuf_fft, n_fft);
#endif /* HAVE_LIBRFFTW */

    /* now outbuf contains y_k(f) */

    /* compute DC case */
    tmpr = params->fft.outbuf[0] - mu[0] * U0[j];	/* real part */
    ftest[0] += tmpr * tmpr;	/* this is actually the denominator */
    for (i = 1; i < (n_fft + 1) / 2; i++) {
      tmpr = params->fft.outbuf[i] - mu[i] * U0[j];	/* real part */
      tmpi = params->fft.outbuf[n_fft - i] - mu[n_fft - i] * U0[j];	/* imag part */
      ftest[i] += tmpr * tmpr + tmpi * tmpi;	/* this is actually the denominator */
    }

    fft_psd(psdbuftmp, NULL, &params->fft);
    /* add weighted eigenspectrum estimate */
    for (i = 0; i < (n_fft + 1) / 2; i++) {
      psd_buf[i] += psdbuftmp[i] / (1.0 + params->sig[j]);
    }
    if (n_fft % 2 == 0) {
      psd_buf[n_fft / 2] += psdbuftmp[n_fft / 2] / (1.0 + params->sig[j]);
    }
  }

  /* compute DC case */
  num_ftest = k * (mu[0] * mu[0]) * sum_U0_sqr;
  ftest[0] = num_ftest / ftest[0];
  for (i = 1; i < (n_fft + 1) / 2; i++) {
    num_ftest = k * (mu[i] * mu[i] + mu[n_fft - i] * mu[n_fft - i]) * sum_U0_sqr;
    ftest[i] = num_ftest / ftest[i];
  }
  if (n_fft % 2 == 0) {
    i = n_fft / 2;
    num_ftest = k * (mu[i] * mu[i] + mu[n_fft - i] * mu[n_fft - i]) * sum_U0_sqr;
    ftest[i] = num_ftest / ftest[i];
  }
/*
  for (i = 0; i < (n_fft + 1) / 2; i++) {
    psd_buf[i] = mu[i] * mu[i];
    //psd_buf[i] = ftest[i];
  }
*/}


void mtm_close(mtm_params_t * params)
{
  int n = params->fft.n;
  int kmax = params->kmax;

  FREE_MAYBE(params->fft.inbuf_audio);
  FREE_MAYBE(params->fft.inbuf_fft);
#ifdef HAVE_LIBRFFTW
  rfftw_destroy_plan(params->fft.plan);
  FREE_MAYBE(params->fft.outbuf);
#endif /* HAVE_LIBRFFTW */
  FREE_MAYBE(psdbuftmp);

  FREE_MAYBE(hn);
  FREE_MAYBE(mu);
  FREE_MAYBE(ftest);

  free_dmatrix(params->window, 1, n, 0, kmax);
  params->window = NULL;
  free_dvector(params->sig, 0, kmax);
  params->sig = NULL;
  free_dvector(U0, 0, kmax);
  U0 = NULL;
}
