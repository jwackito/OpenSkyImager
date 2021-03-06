/*
 * gtkversions.h
 *
 *  Created on: 01.09.2013
 *      Author: Giampiero Spezzano (gspezzano@gmail.com)
 *
 * This file is part of "OpenSkyImager".
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if GLIB_MINOR_VERSION < 32
	#define g_rw_lock_init(lock)                      g_static_rw_lock_init(lock)
	#define g_rw_lock_clear(lock)                     g_static_rw_lock_clear(lock)
	#define g_rw_lock_writer_lock(lock)               g_static_rw_lock_writer_lock(lock)
	#define g_rw_lock_writer_trylock(lock)            g_static_rw_lock_writer_trylock(lock)
	#define g_rw_lock_writer_unlock(lock)             g_static_rw_lock_writer_unlock(lock)
	#define g_rw_lock_reader_lock(lock)               g_static_rw_lock_reader_lock(lock)
	#define g_rw_lock_reader_trylock(lock)            g_static_rw_lock_reader_trylock(lock)
	#define g_rw_lock_reader_unlock(lock)             g_static_rw_lock_reader_unlock(lock)
	#define g_thread_try_new(name, func, data, error) g_thread_create(func, data, TRUE, error)
	#define g_thread_new(name, func, data)            g_thread_create(func, data, TRUE, NULL)
#endif

#if GTK_MAJOR_VERSION == 3
	#define gtk_hscale_new_with_range(min,max,step)                 gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,min,max,step)
	#define gtk_vscale_new_with_range(min,max,step)                 gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,min,max,step)
	#define gtk_hseparator_new()                                    gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)
	#define gtk_vseparator_new()                                    gtk_separator_new(GTK_ORIENTATION_VERTICAL)
	#define gtk_hbox_new(hom,spc)                                   gtk_box_new(GTK_ORIENTATION_HORIZONTAL,spc)
	#define gtk_vbox_new(hom,spc)                                   gtk_box_new(GTK_ORIENTATION_VERTICAL,spc)
	#define gtk_hpaned_new()                                        gtk_paned_new_with_handle(GTK_ORIENTATION_HORIZONTAL)
	#define gtk_vpaned_new()                                        gtk_paned_new_with_handle(GTK_ORIENTATION_VERTICAL)
	#define gtk_table_new(rows,cols,hom)                            gtk_grid_new()
	#ifdef GTK_TABLE
		#undef GTK_TABLE
	#endif
	#define GTK_TABLE(tbl)                                          GTK_GRID(tbl)
	#define gtk_table_attach(tbl, wgt, l, r, t, b, hf, vf, xp, yp)  gtk_grid_attach(tbl, wgt, l, t, r-l, b-t)
	#define gtk_table_set_row_spacings(tbl, spc)                    gtk_grid_set_row_spacing(tbl, spc)
	#define gtk_table_set_col_spacings(tbl, spc)                    gtk_grid_set_column_spacing(tbl, spc)
#elif GTK_MAJOR_VERSION == 2
	#if GTK_MINOR_VERSION < 24
		#define gtk_combo_box_text_new()                           gtk_combo_box_entry_new_text()
		#define gtk_combo_box_text_new_with_entry()                gtk_combo_box_entry_new_text()
		#define GTK_COMBO_BOX_TEXT(cmb)                            GTK_COMBO_BOX(cmb)
		#define gtk_combo_box_text_append_text(cmb, val)           gtk_combo_box_append_text(cmb, val)
		#define gtk_combo_box_text_get_active_text(cmb)            gtk_combo_box_get_active_text(cmb)
		#define gtk_statusbar_remove_all(stb, id)                  gtk_statusbar_pop(stb, id)
	#endif
#endif


