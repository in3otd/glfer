/* util.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>		/* for gettimeofday() */
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <unistd.h>		/* for gettimeofday() */
#include <string.h>
#include "util.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif


#define GSL_DBL_EPSILON 2.22e-16
#define GSL_SQRT_DBL_EPSILON 1.5e-8


#define NR_END 1
#define FREE_ARG char*


static double start_secnds;
static double secnds;
static struct timeval tv;

#ifndef HAVE_STRDUP
char *strdup(const char *s)
{
  int l = strlen(s);
  char *s1 = malloc(l + 1);
  if (!s1)
    memfatal("strdup");
  memcpy(s1, s, l + 1);
  return s1;
}
#endif /* HAVE_STRDUP */


int tc_zero(void)
{
#ifdef HAVE_GETTIMEOFDAY
  if (gettimeofday(&tv, NULL))
    perror("gettimeofday");
  start_secnds = tv.tv_sec + tv.tv_usec / 1.0e06;
  D(printf("tc_zero: tv.tv_sec = %li\n", tv.tv_sec));	/* for debug */
  D(printf("tc_zero: tv.tv_usec = %li\n", tv.tv_usec));	/* for debug */
  D(printf("tc_zero: start_secnds = %f\n", start_secnds));	/* for debug */

#endif /* HAVE_GETTIMEOFDAY */
  return 0;
}


double tc_time(void)
{
#ifdef HAVE_GETTIMEOFDAY
  if (gettimeofday(&tv, NULL))
    perror("gettimeofday");
  secnds = tv.tv_sec + tv.tv_usec / 1.0e06;	// - start_secnds;

  D(printf("start_secnds = %f\n", start_secnds));	/* for debug */
  D(printf("secnds = %f\n", secnds));	/* for debug */

  return secnds;
#else
  return 0;
#endif /* HAVE_GETTIMEOFDAY */
}


void nrerror(char error_text[])
/* Numerical Recipes standard error handler */
{
  fprintf(stderr, "Numerical Recipes run-time error...\n");
  fprintf(stderr, "%s\n", error_text);
  fprintf(stderr, "...now exiting to system...\n");
  exit(1);
}


float *vector(long nl, long nh)
/* allocate a float vector with subscript range v[nl..nh] */
{
  float *v;

  v = (float *) malloc((size_t) ((nh - nl + 1 + NR_END) * sizeof(float)));
  if (!v)
    nrerror("allocation failure in vector()");
  return v - nl + NR_END;
}


double *dvector(long nl, long nh)
/* allocate a double vector with subscript range v[nl..nh] */
{
  double *v;

  v = (double *) malloc((size_t) ((nh - nl + 1 + NR_END) * sizeof(double)));
  if (!v)
    nrerror("allocation failure in dvector()");
  return v - nl + NR_END;
}

float **matrix(long nrl, long nrh, long ncl, long nch)
/* allocate a float matrix with subscript range m[nrl..nrh][ncl..nch] */
{
  long i, nrow = nrh - nrl + 1, ncol = nch - ncl + 1;
  float **m;

  /* allocate pointers to rows */
  m = (float **) malloc((size_t) ((nrow + NR_END) * sizeof(float *)));
  if (!m)
    nrerror("allocation failure 1 in matrix()");
  m += NR_END;
  m -= nrl;

  /* allocate rows and set pointers to them */
  m[nrl] = (float *) malloc((size_t) ((nrow * ncol + NR_END) * sizeof(float)));
  if (!m[nrl])
    nrerror("allocation failure 2 in matrix()");
  m[nrl] += NR_END;
  m[nrl] -= ncl;

  for (i = nrl + 1; i <= nrh; i++)
    m[i] = m[i - 1] + ncol;

  /* return pointer to array of pointers to rows */
  return m;
}

double **dmatrix(long nrl, long nrh, long ncl, long nch)
/* allocate a double matrix with subscript range m[nrl..nrh][ncl..nch] */
{
  long i, nrow = nrh - nrl + 1, ncol = nch - ncl + 1;
  double **m;

  /* allocate pointers to rows */
  m = (double **) malloc((size_t) ((nrow + NR_END) * sizeof(double *)));
  if (!m)
    nrerror("allocation failure 1 in matrix()");
  m += NR_END;
  m -= nrl;

  /* allocate rows and set pointers to them */
  m[nrl] = (double *) malloc((size_t) ((nrow * ncol + NR_END) * sizeof(double)));
  if (!m[nrl])
    nrerror("allocation failure 2 in matrix()");
  m[nrl] += NR_END;
  m[nrl] -= ncl;

  for (i = nrl + 1; i <= nrh; i++)
    m[i] = m[i - 1] + ncol;

  /* return pointer to array of pointers to rows */
  return m;
}


void free_vector(float *v, long nl, long nh)
/* free a float vector allocated with vector() */
{
  free((FREE_ARG) (v + nl - NR_END));
}


void free_dvector(double *v, long nl, long nh)
/* free a double vector allocated with dvector() */
{
  free((FREE_ARG) (v + nl - NR_END));
}

void free_matrix(float **m, long nrl, long nrh, long ncl, long nch)
/* free a float matrix allocated by matrix() */
{
  free((FREE_ARG) (m[nrl] + ncl - NR_END));
  free((FREE_ARG) (m + nrl - NR_END));
}

void free_dmatrix(double **m, long nrl, long nrh, long ncl, long nch)
/* free a double matrix allocated by dmatrix() */
{
  free((FREE_ARG) (m[nrl] + ncl - NR_END));
  free((FREE_ARG) (m + nrl - NR_END));
}


double bessel_I0(double x)
{				/* modified Bessel function of the first kind */
  double ax, ans;
  double y;

  if ((ax = fabs(x)) < 3.75) {
    y = x / 3.75;
    y *= y;
    ans = 1.0 + y * (3.5156229 + y * (3.0899424 + y * (1.2067492 + y * (0.2659732 + y * (0.360768e-01 + y * 0.45813e-02)))));
  } else {
    y = 3.75 / ax;
    ans = (exp(ax) / sqrt(ax)) * (0.39894228 + y * (0.1328592e-01 + y * (0.225319e-02 + y * (-0.157565e-02 + y * (0.916281e-02 + y * (-0.2057706e-01 + y * (0.2635537e-01 + y * (-0.1647633e-01 + y * 0.392377e-02))))))));
  }

  return ans;
}


/* Author:  G. Jungman */
/* This is a the jacobi version */
/*
 * Algorithm due to J.C. Nash, Compact Numerical Methods for
 * Computers (New York: Wiley and Sons, 1979), chapter 3.
 * See also Algorithm 4.1 in
 * James Demmel, Kresimir Veselic, "Jacobi's Method is more
 * accurate than QR", Lapack Working Note 15 (LAWN15), October 1989.
 * Available from netlib.
 *
 * Based on code by Arthur Kosowsky, Rutgers University
 *                  kosowsky@physics.rutgers.edu  
 *
 * Another relevant paper is, P.P.M. De Rijk, "A One-Sided Jacobi
 * Algorithm for computing the singular value decomposition on a
 * vector computer", SIAM Journal of Scientific and Statistical
 * Computing, Vol 10, No 2, pp 359-371, March 1989.
 * 
 */


int compute_svd(float **A, int nrow, int ncol, float S[], float **Q)
/* Given a matrix A[0..nrow-1][0..ncol-1], this routine computes its singular
   values decomposition A=U*S*(Q**T). The matrix U replaces A on output.
   The diagonal matrix of singular values S is output as a vector S[0..ncol-1].
   The matrix Q (not the transpose (Q**T)) is output as Q[0..ncol-1][0..ncol-1]

*/
{
  double tolerance = 1.0e-12;
  int i, j, k;
  /* Initialize the rotation counter and the sweep counter. */
  int count = 1;
  int sweep = 0;
  int sweepmax = ncol;

  if (A == NULL || S == NULL || Q == NULL) {
    return (-1);
  } else if (nrow == 0 || ncol == 0) {
    return (-1);
  } else {

    /* Always do at least 12 sweeps. */
    sweepmax = (sweepmax > 12 ? sweepmax : 12);

    /* Set Q to the identity matrix. */
    for (i = 0; i < ncol; i++) {
      for (j = 0; j < ncol; j++) {
	Q[i][j] = 0.0;
      }
      Q[i][i] = 1.0;
    }

    /* Orthogonalize A by plane rotations. */
    while (count > 0 && sweep <= sweepmax) {

      /* Initialize rotation counter. */
      count = ncol * (ncol - 1) / 2;

      for (j = 0; j < ncol - 1; j++) {
	for (k = j + 1; k < ncol; k++) {
	  double p = 0.0;
	  double q = 0.0;
	  double r = 0.0;
	  double cosine, sine;
	  double v;

	  for (i = 0; i < nrow; i++) {
	    /* quantities in rotation angles */
	    const double Aij = A[i][j];
	    const double Aik = A[i][k];
	    p += Aij * Aik;
	    q += Aij * Aij;
	    r += Aik * Aik;
	  }

	  if (q * r < GSL_DBL_EPSILON) {
	    /* column elements of A small */
	    count--;
	    continue;
	  }
	  if (p * p / (q * r) < tolerance) {
	    /* columns j,k orthogonal */
	    count--;
	    continue;
	  }
	  /* calculate rotation angles */
	  if (q < r) {
	    cosine = 0.0;
	    sine = 1.0;
	  } else {
	    q -= r;
	    v = sqrt(4.0 * p * p + q * q);
	    cosine = sqrt((v + q) / (2.0 * v));
	    sine = p / (v * cosine);
	  }

	  /* apply rotation to A */
	  for (i = 0; i < nrow; i++) {
	    double Aik = A[i][k];
	    double Aij = A[i][j];
	    A[i][j] = Aij * cosine + Aik * sine;
	    A[i][k] = -Aij * sine + Aik * cosine;
	  }
	  /* apply rotation to Q */
	  for (i = 0; i < ncol; i++) {
	    double Qij = Q[i][j];
	    double Qik = Q[i][k];
	    Q[i][j] = Qij * cosine + Qik * sine;
	    Q[i][k] = -Qij * sine + Qik * cosine;
	  }
	}
      }

      /* Sweep completed. */
      sweep++;
    }

/* 
 * Orthogonalization complete. Compute singular values.
 */

    if (count > 0) {
      /* reached sweep limit */
    }
    for (j = 0; j < ncol; j++) {
      double q = 0.0;

      /* Calculate singular values. */
      for (i = 0; i < nrow; i++) {
	double Aij = A[i][j];
	q += Aij * Aij;
      }
      S[j] = sqrt(q);

      /* Normalize vectors. */
      for (i = 0; i < nrow; i++) {
	double Aij = A[i][j];
	double Sj = S[j];
	A[i][j] = Aij / Sj;
      }
    }

    return 0;			/* success */
  }

}

void dump_octave_file(char *name, float *a, int nrows, int ncols)
{
  FILE *fout;
  int i, j;

  fout = fopen(name, "w");

  if (!fout) {			/* problems opening file */
    fprintf(stderr, "cannot open %s.\n", name);
    return;
  }

  fprintf(fout, "# name: %s\n", name);
  fprintf(fout, "# type: matrix\n");
  fprintf(fout, "# rows: %i\n", nrows);
  fprintf(fout, "# columns: %i\n", ncols);

  for (i = 0; i < nrows; i++) {
    for (j = 0; j < ncols; j++) {
//      fprintf(fout, "%f ", a[i][j]);
    }
    fprintf(fout, "\n");
  }

  fclose(fout);
}
