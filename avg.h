/* avg.h
 * 
 * Copyright (C) 2006 Edouard Griffiths (F4EXB)
 * Copyright (C) 2006-2008 Claudio Girardi
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

#ifndef _AVG_H_
#define _AVG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct
{
  int    avgwidth;   /* spectrum width to average, in pixels */
  int    avgdepth;   /* number of spectra to average */
  int    effdepth;   /* number of spectra effectively averaged sofar */
  double *avg;       /* current average array */
  double *cum;       /* current cumulative sum array */
  double **avgarray; /* record array */
} avg_data_t;

extern void init_avg(avg_data_t *avgdata);
extern void alloc_avg(avg_data_t *avgdata, int width, int depth);
extern void delete_avg(avg_data_t *avgdata);
extern double update_avg_plain(avg_data_t *avgdata, int N, float *psd, int minbin, int maxbin, int *peakbin);
extern double update_avg_sumextreme(avg_data_t *avgdata, int N, float *psd, int max0, int minbin, int maxbin, int *peakbin);
extern double update_avg_sumavg(avg_data_t *avgdata, int N, float *psd, int max0, int minbin, int maxbin, int *peakbin, double *variance);

#endif /* #ifndef _AVG_H_ */
