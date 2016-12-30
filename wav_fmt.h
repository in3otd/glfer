/* wav_fmt.h
 * 
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

#ifndef _WAV_FMT_H_
#define _WAV_FMT_H_

int open_wav_file(char *fname, int n, int *speed);
void close_wav_file(void);
void wav_read(float **buf_out, int *n_out);

/* Definitions for Microsoft WAVE format */

/* it's in chunks like .voc and AMIGA iff, but my source say there
   are in only in this combination, so I combined them in one header;
   it works on all WAVE-file I have
*/
typedef struct wavhead
{
  u_long main_chunk;		/* 'RIFF' */
  u_long length;		/* Length of rest of file */
  u_long chunk_type;		/* 'WAVE' */

  u_long sub_chunk;		/* 'fmt ' */
  u_long sc_len;		/* length of sub_chunk, =16 (rest of chunk) */
  u_short format;		/* should be 1 for PCM-code */
  u_short modus;		/* 1 Mono, 2 Stereo */
  u_long sample_fq;		/* frequence of sample */
  u_long byte_p_sec;
  u_short byte_p_spl;		/* samplesize; 1 or 2 bytes */
  u_short bit_p_spl;		/* 8, 12 or 16 bit */

  u_long data_chunk;		/* 'data' */
  u_long data_length;		/* samplecount (lenth of rest of block?) */
}
wavhead;

#endif /* #ifndef _WAV_FMT_H_ */
