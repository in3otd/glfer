/* hparma.c

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
#include "hparma.h"

extern opt_t opt;

static float *a = NULL, *s = NULL, **v = NULL;

static float **r_xx = NULL;


void hparma_init(hparma_params_t * params)
{
  int n = params->fft.n;
  int t = params->t;
  int p_e = params->p_e;

#ifdef HAVE_LIBRFFTW
  params->fft.plan = rfftw_create_plan(n, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);

  params->fft.inbuf_audio = calloc(n, sizeof(fftw_real));
  params->fft.inbuf_fft = calloc(n, sizeof(fftw_real));
  params->fft.outbuf = calloc(n, sizeof(fftw_real));
#else
  params->fft.inbuf_audio = calloc(n, sizeof(float));
  params->fft.inbuf_fft = calloc(n, sizeof(float));
  params->fft.outbuf = params->fft.inbuf_fft;
#endif /* HAVE_LIBRFFTW */

  params->fft.sub_mean = opt.autoscale;  // sub_mean only if autoscale (?)

  r_xx = matrix(0, t, 0, p_e);

  s = vector(0, t);
  v = matrix(0, p_e, 0, p_e);

  a = vector(0, p_e);
}


void hparma_do(float *audio_buf, float *psd_buf, float *phase_buf, hparma_params_t * params)
{
  int i, j, k;
  int n_fft = params->fft.n;
  double ftmp, numtmp, dentmp, nu, sum_sigma2;
  int t = params->t;
  int p_e = params->p_e;
  int q_e = params->q_e;

  int p = 4;

  /* apply overlap, limting, windowing, etc. according to active options */
  prepare_audio(audio_buf, &params->fft);

  /* compute autocorrelation vector */
  for (i = 0; i <= q_e + t; i++) {
    ftmp = 0.0;
    for (k = 0; k < n_fft - i; k++) {
      ftmp += params->fft.inbuf_audio[k + i] * params->fft.inbuf_audio[k];
    }
    r_xx[0][i] = ftmp / (n_fft - i);
  }

  /* fill the symmetric Toeplitz autocorrelation matrix */
  for (i = 1; i < t; i++) {
    for (j = 0; j <= p_e; j++) {
      r_xx[i][j] = r_xx[0][abs(j - i)];
    }
  }

  compute_svd(r_xx, t, p_e + 1, s, v);

  /* compute the sum of eigenvalues squared */
  /* note that p_e + 1 <= t, so there are at most p_e + 1 nozero eigenvalues */
  sum_sigma2 = 0.0;
  for (i = 0; i < p_e + 1; i++) {
    sum_sigma2 += s[i] * s[i];
  }

  ftmp = 0.0;
  for (i = 0; i < p_e + 1; i++) {
    ftmp += s[i] * s[i];
    nu = sqrt(ftmp / sum_sigma2);
//    printf("s[%i] = %e\tnu[%i] = %f\tsumsigma2 = %e \n", i, s[i], i, nu, sum_sigma2);
    if (nu > 0.995) {
      p = i;
      break;
    }
  }

  /* p=3; */
  for (i = 0; i <= p_e; i++) {
    numtmp = dentmp = 0.0;
    for (k = p + 1; k <= p_e; k++) {
      numtmp += v[0][k] * v[i][k];
      dentmp += v[0][k] * v[0][k];
    }
    if (p < p_e) {
      a[i] = numtmp / dentmp;
    } else {
      a[i] = (i == 0 ? 1.0 : 0.0);
    }

//    printf("a[%i] = %e = %e/%e\n", i, a[i], numtmp,  dentmp);
  }

  for (i = 0; i <= p_e; i++) {
    params->fft.inbuf_fft[i] = a[i];
  }
  for (i = p_e + 1; i < n_fft; i++) {
    params->fft.inbuf_fft[i] = 0.0;
  }
  /* compute the fft */
#ifdef HAVE_LIBRFFTW
  rfftw_one(params->fft.plan, params->fft.inbuf_fft, params->fft.outbuf);
#else
    fft_real_radix2_transform(params->fft.inbuf_fft, n_fft);
#endif /* HAVE_LIBRFFTW */

  fft_psd(psd_buf, NULL, &params->fft);
  for (i = 0; i < n_fft / 2; i++) {
    psd_buf[i] = 1.0 / psd_buf[i];
  }
}


void hparma_close(hparma_params_t * params)
{
  int t = params->t;
  int p_e = params->p_e;

  FREE_MAYBE(params->fft.inbuf_audio);
  FREE_MAYBE(params->fft.inbuf_fft);
#ifdef HAVE_LIBRFFTW
  rfftw_destroy_plan(params->fft.plan);
  FREE_MAYBE(params->fft.outbuf);
#endif /* HAVE_LIBRFFTW */
  FREE_MAYBE(params->fft.window);

  free_matrix(r_xx, 0, t, 0, p_e);

  free_vector(s, 0, t);
  free_matrix(v, 0, p_e, 0, p_e);

  free_vector(a, 0, p_e);
}
