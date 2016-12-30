/* wav_fmt.c

 * Copyright (C) 2001 Claudio Girardi
 * This file is derived from bplay, (C) David Monro 1996
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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "util.h"
#include "wav_fmt.h"

#define MSPEED	1
#define MBITS	2
#define MSTEREO	4

int speed, bits, stereo;	/* Audio parameters */

static unsigned char *buf = NULL, *buf8 = NULL;
static short int *buf16 = NULL;
static float *buff = NULL;

static wavhead wavhd;
static int fd_in;
static int out_len = 1024;	//FIXME


int open_wav_file(char *fname, int n, int *speed)
{
  int count;
  char hd_buf[20];		/* Holds first 20 bytes */

  /* sets size of data block to be read */
  out_len = n;
  D(fprintf(stderr, "out_len = %i\n", out_len);)
  if ((fd_in = open(fname, O_RDONLY)) == -1) {
    fprintf(stderr, "error opening %s\n", fname);
    exit(-1);
  }

  count = read(fd_in, hd_buf, 20);
  if (count < 0)
    fprintf(stderr, "error reading WAV file\n");
  if (count < 20)
    fprintf(stderr, "input file less than 20 bytes long\n");
  if (strstr(hd_buf, "RIFF") == NULL)
    fprintf(stderr, "input file not in WAV format\n");

  memcpy((void *) &wavhd, (void *) hd_buf, 20);

  count = read(fd_in, ((char *) &wavhd) + 20, sizeof(wavhd) - 20);
  if (wavhd.format != 1)
    fprintf(stderr, "input is not a PCM WAV file");
  *speed = wavhd.sample_fq;
  bits = wavhd.bit_p_spl;
  stereo = wavhd.modus - 1;
  D(fprintf(stderr, "WAVE format: %d bit, Speed %d %s ...\n", bits, *speed, (stereo) ? "Stereo" : "Mono");)

  return fd_in;
}


/* reads the audio data */
void wav_read(float **buf_out, int *n_out)
{
  int i;
  int n_read;
  int s_bufsize;

  s_bufsize = out_len * wavhd.bit_p_spl / 8;

  if (!buf) {
    switch (wavhd.bit_p_spl) {
    case (8):
      buf = buf8 = (unsigned char *) calloc(out_len, sizeof(unsigned char));
      break;
    case (16):
      buf16 = (short int *) calloc(out_len, sizeof(short int));
      buf = (unsigned char *) buf16;
      break;
    }
    buff = (float *) calloc(out_len, sizeof(float));
  }

  n_read = read(fd_in, buf, s_bufsize);

  switch (wavhd.bit_p_spl) {
  case (8):
    for (i = 0; i < n_read; i++)
      /* buf8 is an array of char */
      buff[i] = ((float) buf8[i] - 128) / 128;
    break;
  case (16):
    for (i = 0; i < n_read / 2; i++) {
      /* buf16[] is an array of short int */
      buff[i] = (float) buf16[i] / 32768;
      //buff[i] = (buf16[i] / 256) + (buf16[i] % 256) * 256;
    }
    break;
  }

  *n_out = (n_read == 0 ? 0 : 1);
  *buf_out = buff;
}

void close_wav_file()
{
  D(printf("%s: buf16 = %p\n", __PRETTY_FUNCTION__, buf16));	/* for debug */

  close(fd_in);

  FREE_MAYBE(buf8);
  FREE_MAYBE(buf16);
  buf = NULL;
  /* since buf is the same as buf8 or buf16 it should not be freed */
  FREE_MAYBE(buff);
}
