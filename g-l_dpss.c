/* g-l_dpss.c
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


/* eigen/jacobi.c
 * eigen/eigen_sort.c
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 Gerard Jungman
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "util.h"


/* The eigen_sort below is not very good, but it is simple and
 * self-contained. We can always implement an improved sort later.  */

int eigen_symmv_sort(double *eval, double **evec, unsigned int N)
{
  size_t i;

  for (i = 0; i < N - 1; i++) {
    size_t j;
    size_t k = i;

    double ek = eval[i];
    double dtmp;

    /* search for something to swap */
    for (j = i + 1; j < N; j++) {
      int test;
      const double ej = eval[j];
      test = (fabs(ej) > fabs(ek));
      if (test) {
	k = j;
	ek = ej;
      }
    }

    if (k != i) {
      /* swap eigenvalues */
      dtmp = eval[i];
      eval[i] = eval[k];
      eval[k] = dtmp;
      /* swap eigenvectors */
      for (j = 0; j < N; j++) {
	dtmp = evec[j][i];
	evec[j][i] = evec[j][k];
	evec[j][k] = dtmp;
      }
    }
  }

  return 0;			/* success */
}


static void jac_rotate(double **a, unsigned int i, unsigned int j, unsigned int k, unsigned int l, double *g, double *h, double s, double tau)
{
  *g = a[i][j];
  *h = a[k][l];
  a[i][j] = (*g) - s * ((*h) + (*g) * tau);
  a[k][l] = (*h) + s * ((*g) - (*h) * tau);
}


int eigen_jacobi(double **a, double *eval, double **evec, unsigned int n, unsigned int max_rot, unsigned int *nrot)
{
  unsigned int i, j, iq, ip;
  double t, s;

  double *b = (double *) malloc(n * sizeof(double));
  double *z = (double *) malloc(n * sizeof(double));
  if (b == 0 || z == 0) {
    if (b != 0)
      free(b);
    if (z != 0)
      free(z);
    fprintf(stderr, "could not allocate memory for workspace");
    exit(-1);			/* out of memory */
  }

  /* Set eigenvectors to coordinate basis. */
  for (ip = 0; ip < n; ip++) {
    for (iq = 0; iq < n; iq++) {
      evec[ip][iq] = 0.0;
    }
    evec[ip][ip] = 1.0;
  }

  /* Initialize eigenvalues and workspace. */
  for (ip = 0; ip < n; ip++) {
    double a_ipip = a[ip][ip];
    z[ip] = 0.0;
    b[ip] = a_ipip;
    eval[ip] = a_ipip;
  }

  *nrot = 0;

  for (i = 1; i <= max_rot; i++) {
    double thresh;
    double tau;
    double g, h, c;
    double sm = 0.0;
    /* compute the sum of the off-diagonal elements absolute value */
    for (ip = 0; ip < n - 1; ip++) {
      for (iq = ip + 1; iq < n; iq++) {
	sm += fabs(a[ip][iq]);
      }
    }
    if (sm == 0.0) {
      /* converged */
      free(z);
      free(b);
      return 0;			/* success */
    }

    if (i < 4)
      thresh = 0.2 * sm / (n * n);
    else
      thresh = 0.0;

    for (ip = 0; ip < n - 1; ip++) {
      for (iq = ip + 1; iq < n; iq++) {
	const double d_ip = eval[ip];
	const double d_iq = eval[iq];
	const double a_ipiq = a[ip][iq];
	g = 100.0 * fabs(a_ipiq);
	/* after for sweeps, skip the rotation for small off-diagonal element */
	if ((i > 4) && (fabs(d_ip) + g == fabs(d_ip)) && (fabs(d_iq) + g == fabs(d_iq))) {
	  a[ip][iq] = 0.0;
	} else if (fabs(a_ipiq) > thresh) {
	  h = d_iq - d_ip;
	  if (fabs(h) + g == fabs(h)) {
	    t = a_ipiq / h;
	  } else {
	    double theta = 0.5 * h / a_ipiq;
	    t = 1.0 / (fabs(theta) + sqrt(1.0 + theta * theta));
	    if (theta < 0.0)
	      t = -t;
	  }

	  c = 1.0 / sqrt(1.0 + t * t);
	  s = t * c;
	  tau = s / (1.0 + c);
	  h = t * a_ipiq;
	  z[ip] -= h;
	  z[iq] += h;
	  eval[ip] = d_ip - h;
	  eval[iq] = d_iq + h;
	  a[ip][iq] = 0.0;

	  for (j = 0; j < ip; j++) {
	    jac_rotate(a, j, ip, j, iq, &g, &h, s, tau);
	  }
	  for (j = ip + 1; j < iq; j++) {
	    jac_rotate(a, ip, j, j, iq, &g, &h, s, tau);
	  }
	  for (j = iq + 1; j < n; j++) {
	    jac_rotate(a, ip, j, iq, j, &g, &h, s, tau);
	  }
	  for (j = 0; j < n; j++) {
	    jac_rotate(evec, j, ip, j, iq, &g, &h, s, tau);
	  }
	  ++(*nrot);
	}
      }
    }
    for (ip = 0; ip < n; ip++) {
      b[ip] += z[ip];
      z[ip] = 0.0;
      eval[ip] = b[ip];
    }
    /* continue iteration */
  }

  return (-1);			/* max iteration exceeded */
}


/* 
   function [evc,evl] = w2(l,f)
   % [evc,evl] = window2(l,f)
   %   Generate the window functions using the approximation method outlined
   % in Appendix A of Thomson in Proc. of the IEEE, September 1987.  evc is
   % a matrix whose columns are the eigenvectors (windows) with corresponding
   % eigenvalues in the column vector evl.  Only the first 2lf eigenvectors
   % are returned.  l is the number of window terms to compute, and f is the
   % bandwidth.  evl is sorted from small to large eigenvalues.
   %   This program uses a 32-point Gauss-Legendre quadrature (c.f. Appendix A).
   %   By John Kuehne, August 1990.
*/

/* Zeros of Legendre(32) polynomial (abscissas) */
double gl_x[] = {
  -.997263861849481563545,
  -.985611511545268335400,
  -.964762255587506430774,
  -.934906075937739689171,
  -.896321155766052123965,
  -.849367613732569970134,
  -.794483795967942406963,
  -.732182118740289680387,
  -.663044266930215200975,
  -.587715757240762329041,
  -.506899908932229390024,
  -.421351276130635345364,
  -.331868602282127649780,
  -.239287362252137074545,
  -.144471961582796493485,
  -.048307665687738316235,
  .048307665687738316235,
  .144471961582796493485,
  .239287362252137074545,
  .331868602282127649780,
  .421351276130635345364,
  .506899908932229390024,
  .587715757240762329041,
  .663044266930215200975,
  .732182118740289680387,
  .794483795967942406963,
  .849367613732569970134,
  .896321155766052123965,
  .934906075937739689171,
  .964762255587506430774,
  .985611511545268335400,
  .997263861849481563545
};

/* and weights for quadrature */
double gl_w[] = {
  .007018610009470096600,
  .016274394730905670605,
  .025392065309262059456,
  .034273862913021433103,
  .042835898022226680657,
  .050998059262376176196,
  .058684093478535547145,
  .065822222776361846838,
  .072345794108848506225,
  .078193895787070306472,
  .083311924226946755222,
  .087652093004403811143,
  .091173878695763884713,
  .093844399080804565639,
  .095638720079274859419,
  .096540088514727800567,
  .096540088514727800567,
  .095638720079274859419,
  .093844399080804565639,
  .091173878695763884713,
  .087652093004403811143,
  .083311924226946755222,
  .078193895787070306472,
  .072345794108848506225,
  .065822222776361846838,
  .058684093478535547145,
  .050998059262376176196,
  .042835898022226680657,
  .034273862913021433103,
  .025392065309262059456,
  .016274394730905670605,
  .007018610009470096600
};





int gl_dpss(int nmax, int kmax, int n, double w, double **v, double *sig, int *totit)
{
  int i, j, ik;
  int errcode, nrot;
  double **k, *eigen_val, **eigen_vec;
  double dtmp, c;

  /*  c = n * M_PI * w; */
  /* our definition of w is actually N*w */
  c = M_PI * w;

  k = dmatrix(0, 31, 0, 31);
  eigen_val = dvector(0, 31);
  eigen_vec = dmatrix(0, 31, 0, 31);

  for (i = 0; i < 32; i++) {
    for (j = 0; j < 32; j++) {
      if (i == j)
	k[i][j] = c / M_PI;
      else
	k[i][j] = sin(c * (gl_x[i] - gl_x[j])) / (M_PI * (gl_x[i] - gl_x[j]));

      /* Multiply weights to complete k(m,j) */
      k[i][j] *= sqrt(gl_w[i] * gl_w[j]);
    }
  }
  /* compute matrix eigenvalues and eigenvectors */
  errcode = eigen_jacobi(k, eigen_val, eigen_vec, 32, 1000, &nrot);
  eigen_symmv_sort(eigen_val, eigen_vec, 32);

  /* interpolate */
  for (i = 0; i < n; i++) {
    for (ik = 0; ik <= kmax; ik++) {
      dtmp = 0.0;
      for (j = 0; j < 32; j++) {
	double argm = (2.0 * (i + 0.5) / n) - 1.0 - gl_x[j];
	dtmp += sqrt(gl_w[j]) * eigen_vec[j][ik] * sin(c * argm) / (M_PI * argm);
      }
      v[i + 1][ik] = dtmp;
    }
  }

  /* normalize */
  for (ik = 0; ik <= kmax; ik++) {
    dtmp = 0.0;
    for (i = 0; i < n; i++) {
      dtmp += (v[i + 1][ik]) * (v[i + 1][ik]);
    }
    for (i = 0; i < n; i++) {
      v[i + 1][ik] /= sqrt(dtmp);
    }
  }

  /* eigenvalues */
  for (ik = 0; ik <= kmax; ik++) {
    sig[ik] = eigen_val[ik] - 1.0;
  }

  return errcode;
}
