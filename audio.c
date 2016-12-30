/* audio.c
 *
 * Copyright (C) 2001-2007 Claudio Girardi
 *
 * This file is derived from xspectrum, Copyright (C) 2000 Vincent Arkesteijn
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <errno.h>
#include "audio.h"
#include "util.h"

#ifndef AFMT_S16_NE
#include <endian.h>
#if (__BYTE_ORDER==__LITTLE_ENDIAN)
#define AFMT_S16_NE AFMT_S16_LE
#else
#define AFMT_S16_NE AFMT_S16_BE
#endif
#endif

#include "audio.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>		/* dmalloc.h should be included last */
#endif

#define DEFAULT_DEV_NAME "/dev/dsp"
#define DEFAULT_SAMPLE_RATE 44000

static int audio_fd;

static int sample_resolution;	/* # of bits / sample */
static int sample_offset;

static int bufsize;
static int out_len;		/* number of samples to acquire */

static unsigned char *buf = NULL, *buf8 = NULL;
static short int *buf16 = NULL;
static float *buff_f = NULL;
static int old_p = 0, old_len = 0;


/* initialises the audio interface */
int audio_init(char *device, int *sample_rate, int len)
{
  int format, stereo, version, n;

  out_len = len;

  if (!device)
    device = DEFAULT_DEV_NAME;
  if ((audio_fd = open(device, O_RDONLY | O_NONBLOCK)) == -1) {
    perror(device);
    return (-1);
  }
  format = AFMT_S16_NE;
  /*  format = AFMT_U8; */
  if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
    perror("SNDCTL_DSP_SETFMT");
    return (-1);
  }
  switch (format) {
  case (AFMT_U8):
    sample_resolution = 8;
    sample_offset = 128;
    break;
  case (AFMT_S16_NE):
    sample_resolution = 16;
    sample_offset = 0;
    break;
  default:
    format = AFMT_U8;
    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
      perror("SNDCTL_DSP_SETFMT");
      return (-1);
    }
    if (format != AFMT_U8) {
      fprintf(stderr, "Sorry, 8-bit linear audio not supported.\n");
      return (-1);
    }
    sample_resolution = 8;
    sample_offset = 128;
  }

  len *= sample_resolution / 8;

  /* OSS User's Guide says there is no need to set explicitely the fragment size... */
  n = 0x7fff0001;
  while (len > 2) {
    n++;
    len /= 2;
  }
  if (n < 0x7fff0004)		/* minimum allowed value */
    n = 0x7fff0004;
  /* first 16 bits are the max. number of fragments; 0x7fff means no limit */
  /* last 16 bits are the log2 of the fragment size */
  if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &n) == -1) {
    perror("SNDCTL_DSP_SETFRAGMENT");
    return (-1);
  }

  stereo = 0;
  if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1) {
    perror("SNDCTL_DSP_STEREO");
    return (-1);
  }
  if (stereo != 0) {
    fprintf(stderr, "Sorry, mono audio not supported.\n");
  }
  if (!*sample_rate)
    *sample_rate = DEFAULT_SAMPLE_RATE;
  n = *sample_rate;
  if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &n) == -1) {
    perror("SNDCTL_DSP_SPEED");
    return (-1);
  }
  if (n != *sample_rate)
    fprintf(stderr, "warning: sample rate changed (%i -> %i)\n", *sample_rate, n);
  *sample_rate = n;

  if (ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &bufsize) == -1) {
    perror("SNDCTL_DSP_GETBLKSIZE");
    return (-1);
  }
  D(printf("bufsize = %i\n", bufsize));	/* for debug */

  /* Read something (one full fragment), to start recording. This shouldn't
   * be necessary but due to a bug in old versions of OSS, it is. */
#ifdef OSS_GETVERSION
  if (ioctl(audio_fd, OSS_GETVERSION, &version) == -1) {
    if (errno == EINVAL) {	/* this ioctl was introduced in version 3.6.0 */
      version = 0;
    } else {
      perror("OSS_GETVERSION");
      return (-1);
    }
  }
  if (version < 360)
#endif
  {
    unsigned char *dummy;

    dummy = malloc(bufsize);
    if (read(audio_fd, dummy, bufsize) < bufsize)
      perror("audio read");
    FREE_MAYBE(dummy);
  }
  buf = NULL;
  old_p = 0, old_len = 0;

  return audio_fd;
}


/* reads the audio data */
void audio_read(float **buf_out, int *n_out)
{
  int i, s_bufsize, res, n_read;
  short sample;

  /* size of the buffer of char (8 bits) */
  s_bufsize = bufsize * 8 / sample_resolution;
  /* when using 16 bit samples the sample buffer will have */
  /* twice the size of the buffer used for the 8 bit case, */
  /* since we want to be able to store the same number of samples */

  if (!buf) {
    switch (sample_resolution) {
    case (8):
      buf = buf8 = (unsigned char *) calloc(s_bufsize, sizeof(unsigned char));
      break;
    case (16):
      buf16 = calloc(s_bufsize, sizeof(short int));
      buf = (unsigned char *) buf16;
      break;
    }
    buff_f = (float *) calloc(s_bufsize + out_len, sizeof(float));
    if (!buff_f) {
      fprintf(stderr, "error allocating buff_f (size %i)\n", s_bufsize + out_len);
      exit(-1);
    }
  }

  i = 0;
  for (;;) {
    /* read 1 or 2 bytes, depending on the desired sample resolution */
    res = read(audio_fd, &sample, sample_resolution / 8);
    if (res == 0) {		/* no samples available */
      break;
    } else if (res != sample_resolution / 8) {	/* error in reading */
      D(fprintf(stderr, "%d %d %s\n", res, errno, strerror(errno))); /* for debug */

      if (errno == EINTR)
	break;			/* interrupted */ //continue;
      if (errno == EAGAIN || errno == EBUSY)	/* no data was immediately available */
	break;
      /* otherwise... */
      perror("Audio read failed");
      exit(1);
    }
    /* fill the buffer */
    switch (sample_resolution) {
    case (8):
      buf8[i++] = (unsigned char) sample;
      break;
    case (16):
      buf16[i++] = (short int) sample;
      break;
    }

    if (i >= s_bufsize)
      /* should not happen, but... */
      fprintf(stderr, "Error !\n");
      break;
  }
  n_read = i;			/* number of acquired samples */

  /* fprintf(stderr, "n_read = %i, old_len = %i, old_p = %i n_read+old_len = %i %i\n", n_read, old_len, old_p, n_read + old_len, s_bufsize+out_len); */

  /* move the old data to the beginning */
  for (i = 0; i < old_len; i++)
    buff_f[i] = buff_f[old_p + i];
  old_p = 0;


  /* copy the sound driver buffer into the application buffer */
  switch (sample_resolution) {
  case (8):
    for (i = 0; i < n_read; i++)
      /* buf8 is an array of char */
      buff_f[i + old_len] = ((float) buf8[i] - sample_offset) / 128;
    break;
  case (16):
    for (i = 0; i < n_read; i++) {
      /* buf16[] is an array of short int */
      buff_f[i + old_len] = ((float) buf16[i] - sample_offset) / 32768;
    }
    break;
  }

  *buf_out = buff_f;
  *n_out = (old_len + n_read) / out_len;
  old_p = (*n_out) * out_len;
  old_len = (old_len + n_read) % out_len;

  /* return 1; */
}


/* reads the audio data */
void audio_read_old(float **buf_out, int *n_out)
{
  int i, s_bufsize;
  audio_buf_info info;

  s_bufsize = bufsize * 8 / sample_resolution;

  if (!buf) {
    switch (sample_resolution) {
    case (8):
      buf = buf8 = (unsigned char *) calloc(s_bufsize, sizeof(unsigned char));
      break;
    case (16):
      buf16 = (short int *) calloc(s_bufsize, sizeof(short int));
      buf = (unsigned char *) buf16;
      break;
    }
    buff_f = (float *) calloc(s_bufsize + out_len, sizeof(float));
    if (!buff_f) {
      fprintf(stderr, "error allocating buff_f (size %i)\n", s_bufsize + out_len);
      exit(-1);
    }
  }
  for (i = 0; i < old_len; i++)
    buff_f[i] = buff_f[old_p + i];
  old_p = 0;
  
  if (ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
    perror("SNDCTL_DSP_GETISPACE");
    exit(-1);
  }

  if (info.bytes < bufsize) {
    fprintf(stderr, "Not enough data for audio read (%i<%i).\n", info.bytes, bufsize);
    *n_out = 0;
    return;
  }
  
  /* read a full sound driver buffer (as recommended by the OSS) */
  if (read(audio_fd, buf, bufsize) < bufsize) {
    perror("audio read");
    return;
  }
  /* copy the sound driver buffer into the application buffer */
  switch (sample_resolution) {
  case (8):
    for (i = 0; i < bufsize; i++)
      /* buf8 is an array of char */
      buff_f[i + old_len] = ((float) buf8[i] - sample_offset) / 128;
    break;
  case (16):
    for (i = 0; i < bufsize / 2; i++) {
      /* buf16[] is an array of short int */
      buff_f[i + old_len] = ((float) buf16[i] - sample_offset) / 32768;
    }
    break;
  }

  *buf_out = buff_f;
  *n_out = (old_len + s_bufsize) / out_len;
  old_p = (*n_out) * out_len;
  old_len = (old_len + s_bufsize) % out_len;
}


void audio_close(void)
{
  D(printf("buf16 = %p\n", buf16));	/* for debug */

  close(audio_fd);
  FREE_MAYBE(buf8);
  FREE_MAYBE(buf16);
  buf = NULL;
  /* since buf is the same as buf8 or buf16 it should not be freed */
  FREE_MAYBE(buff_f);
}
