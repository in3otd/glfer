/* util.h
 *
 * Copyright (C) 2001 Claudio Girardi
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

#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int _do_nothing(const char *format, ...);


#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif /* HAVE_STRDUP */

/* Free FOO if it is non-NULL.  */
#define FREE_MAYBE(foo) do { \
  if (foo) {\
    free (foo); foo = NULL;\
  } else \
    D(printf("Attempt to free a NULL pointer in %s()\n", __FUNCTION__)); \
} while (0)


int tc_zero(void);

double tc_time(void);

float *vector(long nl, long nh);
double *dvector(long nl, long nh);
float **matrix(long nrl, long nrh, long ncl, long nch);
double **dmatrix(long nrl, long nrh, long ncl, long nch);
void free_vector(float *v, long nl, long nh);
void free_dvector(double *v, long nl, long nh);
void free_matrix(float **m, long nrl, long nrh, long ncl, long nch);
void free_dmatrix(double **m, long nrl, long nrh, long ncl, long nch);

double bessel_I0(double x);

int compute_svd(float **A, int nrow, int ncol, float S[], float **Q);

void dump_octave_file(char *name, float *a, int nrows, int ncols);

#endif /* _UTIL_H_ */
