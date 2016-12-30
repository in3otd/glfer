/* g_scope.h

 * Copyright (C) 2005 Claudio Girardi
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

#ifndef _G_SCOPE_H_
#define _G_SCOPE_H_

#include "fft.h"

enum {AUDIO_SOURCE, FFT_SOURCE};

typedef struct _input_source_t input_source_t;

struct _input_source_t
{
  char *name;
  int type;
};

void scope_window_init(GtkWidget * widget, gpointer data);

void scope_window_draw(fft_params_t params);

void scope_window_close(void);

#endif /* _G_SCOPE_H_ */
