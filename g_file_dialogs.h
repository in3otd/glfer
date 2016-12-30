/* g_file_dialogs.h

 * Copyright (C) 2009 Claudio Girardi
 *
 * This file is derived from xspectrum, Copyright (C) 2000 Vincent Arkesteijn
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

#ifndef _G_FILE_DIALOGS_H_
#define _G_FILE_DIALOGS_H_

#include <gtk/gtk.h>

void open_file_dialog(GtkWidget * widget, gpointer data);
void save_spect_dialog(GtkWidget * widget, gpointer data);

#endif /* #ifndef _G_FILE_DIALOGS_H_ */
