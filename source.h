/* source.h
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

#ifndef _SOURCE_H_
#define _SOURCE_H_

void init_audio(char *fname);
void close_audio(void);
void toggle_stop_start(GtkWidget * widget, gpointer data);
void start_reading_audio();
void stop_reading_audio();
void change_params(int mode_sel);

#endif /* not _SOURCE_H_ */
