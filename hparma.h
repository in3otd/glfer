/* hparma.h
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

#ifndef _HPARMA_H_
#define _HPARMA_H_

#include <fft.h>

typedef struct
{
  fft_params_t fft;
  int t;
  int p_e;
  int q_e;
}
hparma_params_t;

void hparma_init(hparma_params_t * params);

void hparma_do(float *audio_buf, float *psd_buf, float *phase_buf, hparma_params_t * params);

void hparma_close(hparma_params_t * params);

#endif /* not _HPARMA_H_ */
