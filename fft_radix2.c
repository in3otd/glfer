/* fft_radix2.c

 * Copyright (C) 2001 Claudio Girardi
 * 
 * This file is derived from the files in the fft directory of gsl-0.9.3, 
 * which are Copyright (C) 1996-2000 Brian Gough
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static int fft_binary_logn(const size_t n)
{
  size_t ntest;
  size_t binary_logn = 0;
  size_t k = 1;

  while (k < n) {
    k *= 2;
    binary_logn++;
  }

  ntest = (1 << binary_logn);

  if (n != ntest) {
    return -1;			/* n is not a power of 2 */
  }
  return binary_logn;
}


static int fft_real_bitreverse_order(float data[], const size_t n)
{
  /* This is the Gold-Rader bit-reversal algorithm */

  size_t i;
  size_t j = 0;

  for (i = 0; i < n - 1; i++) {
    size_t k = n / 2;

    if (i < j) {
      const float tmp = data[i];
      data[i] = data[j];
      data[j] = tmp;
    }
    while (k <= j) {
      j = j - k;
      k = k / 2;
    }

    j += k;
  }

  return 0;
}



int fft_real_radix2_transform(float data[], const size_t n)
{
  int result;
  size_t p, p_1, q;
  size_t i;
  size_t logn = 0;
  int status;

  if (n == 1) {			/* identity operation */
    return 0;
  }
  /* make sure that n is a power of 2 */
  result = fft_binary_logn(n);

  if (result == -1) {
    fprintf(stderr, "n is not a power of 2");
  } else {
    logn = result;
  }

  /* bit reverse the ordering of input data for decimation in time algorithm */

  status = fft_real_bitreverse_order(data, n);

  /* apply fft recursion */

  p = 1;
  q = n;

  for (i = 1; i <= logn; i++) {
    size_t a, b;

    p_1 = p;
    p = 2 * p;
    q = q / 2;

    /* a = 0 */

    for (b = 0; b < q; b++) {
      float t0_real = data[b * p] + data[b * p + p_1];
      float t1_real = data[b * p] - data[b * p + p_1];

      data[b * p] = t0_real;
      data[b * p + p_1] = t1_real;
    }

    /* a = 1 ... p_{i-1}/2 - 1 */

    {
      float w_real = 1.0;
      float w_imag = 0.0;

      const double theta = -2.0 * M_PI / p;

      const float s = sin(theta);
      const float t = sin(theta / 2.0);
      const float s2 = 2.0 * t * t;

      for (a = 1; a < (p_1) / 2; a++) {
	/* trignometric recurrence for w-> exp(i theta) w */

	{
	  const float tmp_real = w_real - s * w_imag - s2 * w_real;
	  const float tmp_imag = w_imag + s * w_real - s2 * w_imag;
	  w_real = tmp_real;
	  w_imag = tmp_imag;
	}

	for (b = 0; b < q; b++) {
	  float z0_real = data[b * p + a];
	  float z0_imag = data[b * p + p_1 - a];
	  float z1_real = data[b * p + p_1 + a];
	  float z1_imag = data[b * p + p - a];

	  /* t0 = z0 + w * z1 */

	  float t0_real = z0_real + w_real * z1_real - w_imag * z1_imag;
	  float t0_imag = z0_imag + w_real * z1_imag + w_imag * z1_real;

	  /* t1 = z0 - w * z1 */

	  float t1_real = z0_real - w_real * z1_real + w_imag * z1_imag;
	  float t1_imag = z0_imag - w_real * z1_imag - w_imag * z1_real;

	  data[b * p + a] = t0_real;
	  data[b * p + p - a] = t0_imag;

	  data[b * p + p_1 - a] = t1_real;
	  data[b * p + p_1 + a] = -t1_imag;
	}
      }
    }

    if (p_1 > 1) {
      for (b = 0; b < q; b++) {
	/* a = p_{i-1}/2 */

	data[b * p + p - p_1 / 2] *= -1;
      }
    }
  }
  return 0;
}
