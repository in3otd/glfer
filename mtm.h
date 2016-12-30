/* mtm.h
 * 
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

#ifndef _MTM_H_
#define _MTM_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBRFFTW
#include <rfftw.h>
#else
#include "fft_radix2.h"
#endif /* HAVE_LIBRFFTW */

#include "fft.h"


typedef struct
{
  fft_params_t fft;
  double **window;
  double *sig;
  float w;
  int kmax;
}
mtm_params_t;


void mtm_init(mtm_params_t * params);
void mtm_do(float *audio_buf, float *psd_buf, float *phase_buf, mtm_params_t * params);
void mtm_close(mtm_params_t * params);

#endif /* #ifndef _MTM_H_ */
