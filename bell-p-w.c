/* bell-p-w.c
 * 
 * Copyright (C) 2008 Claudio Girardi
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

/*
 * This file is derived from FORTRAN 77 code by Brad Bell, Don Percival 
 * and Andrew Walden. Original note says 
 *   "
      Comments / queries * to dbp@apl.washington.edu or a.walden@ic.ac.uk. 
      This software may be freely used for non-commercial purposes and 
      can be freely distributed.
      Equation numbers and comments refer to the article:
      Bell, B., Percival, D.B. and Walden, A.T. "Calculating Thomson's Spectral
      Multitapers by Inverse Iteration ", J. Comput. and Graph. Stat., 1993.
     "
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "util.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

int sytoep(int n, double *r, double *g, double *f)
{
/* Finds filter corresponding to a symmetric Toeplitz matrix with first row 
   r[] and crosscorrelation vector g[]
   
  i.e., "r" f = g
  
  To be used with DPSS and SPOL. See
  Bell, B., Percival, D.B. and Walden, A.T. "Calculating Thomson's
   Spectral Multitapers by Inverse Iteration", J. Comput. and Graph.
   Stat., 1993.

 +++++++++++++++++++++++++++++++++++++++++++++++++++++++

     N       integer    input:  dimension of Toeplitz matrix and
                                 cross-correlation vector
     R(N)    real       input:  autocovariances from lag 0 to N-1
     G(N)    real       input:  cross-correlation vector
     F(N)    real      output:  required filter
     W(N)    real       input:  work array
     IFAULT  integer   output:  0 indicates successful
                                1 if N < 1

 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

   This program is a substantially corrected and modified version of 
   "EUREKA" in Robinson, E.A. (1967) "Multichannel Time Series Analysis 
   with Digital Computer Programs", Holden-Day.

 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
  int i, l, l1, l2, l3, j, k;
  double *w, v, d, q, hold;

  w = dvector(1, n);

  /* check for special "matrix" sizes */
  if (n < 1)
    return 1;
  v = r[0];
  f[1] = g[1] / v;
  if (n == 1)
    return 0;

  d = r[1];
  w[1] = 1.0;
  q = f[1] * r[1];
  for (l = 2; l <= n; l++) {
    w[l] = -d / v;
    if (l > 2) {
      l1 = (l - 2) / 2;
      l2 = l1 + 1;
      if (l != 3) {
	for (j = 2; j <= l2; j++) {
	  hold = w[j];
	  k = l - j + 1;
	  w[j] = w[j] + w[l] * w[k];
	  w[k] = w[k] + w[l] * hold;
	}
      }
      if (((2 * l1) != (l - 2)) || (l == 3))
	w[l2 + 1] = w[l2 + 1] + w[l] * w[l2 + 1];
    }
    v = v + w[l] * d;
    f[l] = (g[l] - q) / v;
    l3 = l - 1;
    for (j = 1; j <= l3; j++) {
      k = l - j + 1;
      f[j] = f[j] + f[l] * w[k];
    }
    if (l == n)
      return 0;

    d = 0.0;
    q = 0.0;
    for (i = 1; i <= l; i++) {
      k = l - i + 2;
      d = d + w[i] * r[k - 1];
      q = q + f[i] * r[k - 1];
    }
  }

  free_dvector(w, 1, n);;
  return 0;
}


int spol(int n, double *v, int k)
{
/* Scales the discrete prolate spheroidal sequence and sets the 
  polarity to agree with Slepian's convention.

  To be used with DPSS and SYTOEP. See
  Bell, B., Percival, D.B. and Walden, A.T. "Calculating Thomson's
    Spectral Multitapers by Inverse Iteration", J. Comput. and Graph.
    Stat., 1993. (Section 1.2.)

  ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  N      Integer             input        length of dpss sequence
  V      Real(N)             input        eigenvector (dpss) with unit energy
                            output        unit energy dpss conforming to
                                          Slepian's polarity convention
  K      Integer             input        the order of the dpss 0=<K=<N-1
  IFAULT Integer            output        0 indicates successful
                                          1 indicates N < 1

  ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
  int l, ifault;
  double rn, dsum, dwsum;

  if (n < 1)
    return 1;
  ifault = 0;
  rn = n;
  dsum = 0.0;
  dwsum = 0.0;
  for (l = 1; l <= n; l++) {
    dsum = dsum + v[l];
    dwsum = dwsum + v[l] * (rn - 1.0 - 2.0 * (l - 1));
  }
  if ((((k % 2) == 0) && (dsum < 0.0)) || (((k % 2) == 1) && (dwsum < 0.0)))
    for (l = 1; l <= n; l++)
      v[l] = -v[l];

  return ifault;
}


int dpss(int nmax, int kmax, int n, double w, double **v, double *sig, int *totit)
{
/* Calculates discrete prolate spheroidal sequences for use as data tapers.
   Calls auxiliary routines SYTOEP and SPOL also given below.
   
   ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   
   NMAX    integer    input:  maximum possible length of taper
   KMAX    integer    input:  dpss orders 0 to KMAX required
   N       integer    input:  length of sequence to generate
   W       real       input:  half-bandwidth, W < 1/2
   V(NMAX, KMAX+1) real array output:  columns contain tapers
   SIG(KMAX+1)     real array output:  eigenvalues are 1+SIG(j)
   TOTIT   integer   output:  total number of iterations
   SINES(0:N-1) real array          work array
   VOLD(N) real array         work array
   U(N)    real array         work array
   SCR1(N) real array         work array
   IFAULT  integer   output:   0 indicates success
     1 if W > 1/2
     2 if N < 2
     3 if NMAX < N; matrix too small
     4 if KMAX < 0
     5 failure in SYTOEP
     6 > MAXIT its required for some order
     (Output values are undefined for
     IFAULT in the range 1 to 5.)
     
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

  int j, jj, k, k1, m, isig, ilow, ihigh, it, maxit, ifail, ifault;
  double *u, *vold, *sines;
  double *scr1, delta, rootn;
  double proj, snorm, ssnorm, diff, sum, rone;

  double eps = 0.5e-6;

  sines = dvector(0, n - 1);
  vold = dvector(1, n);
  u = dvector(1, n);
  scr1 = dvector(1, n);

  /* initialize max number of iterations flag */
  ifail = 0;

  /* check input parameters */
  if (w > 0.5)
    return 1;
  if (n < 2)
    return 2;
  if (nmax < n)
    return 3;
  if ((kmax < 0) || (kmax > n - 1))
    return 4;

  /* set up SINES so that S in eqn. (1) is given by */
  /* S(n,m)=SINES(n-m) for n not equal to m */
  for (m = 1; m < n; m++)
    sines[m] = sin(2.0 * M_PI * w * m) / (M_PI * m);

  /* set total iteration counter and constant */
  *totit = 0;
  rootn = sqrt(n);
  rone = 1.0 / rootn;

  /* major loop over dpss orders 0 to KMAX */

  /* modify SINES(0) so that B_k in Section 2.2 is given by */
  /* B_k(n,m)=SINES(n-m) */
  for (k = 0; k <= kmax; k++) {
    if (k == 0) {
      sines[0] = 2.0 * w - 1.0;
    } else {
      sines[0] = 2.0 * w - (1.0 + sig[k - 1]);
    }

    /* define suitable starting vector for inverse iteration; */
    /* see Section 2.2 */
    isig = 1;
    k1 = k + 1;
    for (j = 1; j <= k1; j++) {
      ilow = ((j - 1) * n / k1) + 1;
      ihigh = (j * n / k1);
      for (jj = ilow; jj <= ihigh; jj++) {
	u[jj] = isig * rone;
      }
      isig = -isig;
    }
    if (((k % 2) != 0) && ((n % 2) > 0))
      u[n / 2 + 1] = 0.0;

    /* maximum number of iterations */
    maxit = (k + 3) * rootn;

    /* carry out inverse iteration */
    for (it = 1; it <= maxit; it++) {
      /* copy U into old V; VOLD = previous iterate */
      for (j = 1; j <= n; j++)
	vold[j] = u[j];

      /* solve symmetric Toeplitz matrix equation B_k*U=VOLD for U */
      ifail = sytoep(n, sines, vold, u);

      /* check no problems */
      if (ifail != 0)
	return 5;

      /* new vector must be orthogonal to previous eigenvectors */
      if (k > 0) {
	for (k1 = 0; k1 < k; k1++) {
	  /* projection of u onto v(*,k1) */
	  proj = 0.0;
	  for (j = 1; j <= n; j++)
	    proj = proj + u[j] * v[j][k1];

	  /* subtract projection */
	  for (j = 1; j <= n; j++)
	    u[j] = u[j] - proj * v[j][k1];
	}
      }
      /* normalize */
      snorm = 0.0;
      for (j = 1; j <= n; j++)
	snorm = snorm + u[j] * u[j];
      ssnorm = sqrt(snorm);

      for (j = 1; j <= n; j++)
	u[j] = u[j] / ssnorm;

      /* check for convergence */
      sum = 0.0;
      diff = 0.0;
      for (j = 1; j <= n; j++) {
	/* first previous-current */
	diff = diff + (vold[j] - u[j]) * (vold[j] - u[j]);
	/* next, previous+current */
	sum = sum + (vold[j] + u[j]) * (vold[j] + u[j]);
      }
      delta = sqrt(MIN(diff, sum));

      if (delta < eps) {
	ifail = 0;
	goto endloop;
      }
    }


    /* FIXME */
    /* if here, max number of iterations exceeded for this order dpss */
    it = maxit;
    ifail = 1;

  endloop:
    *totit += it;
    if (sum < diff) {
      if (k == 0) {
	sig[0] = -1.0 / ssnorm;
      } else {
	sig[k] = sig[k - 1] - 1.0 / ssnorm;
      }
    } else {
      if (k == 0) {
	sig[0] = 1.0 / ssnorm;
      } else {
	sig[k] = sig[k - 1] + 1.0 / ssnorm;
      }
    }

    /* ensure tapers satisfy Slepian convention */
    ifault = spol(n, u, k);
    for (j = 1; j <= n; j++)
      v[j][k] = u[j];

  }
  /* one order of dpss did not converge set IFAULT to 6 */
  if (ifail == 1)
    return 6;
  else
    return 0;
}
