/*
 * imgWFuncs.c
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

#include "imgWindow.h"
#include "imgWFuncs.h"
#include "imgWCallbacks.h"
#include <sys/time.h>

void get_filename(char **filename, int mode, char* flt)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(C_("dialog-open","Open File"), (GtkWindow *) window, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, ((mode == 1) ? GTK_STOCK_SAVE : GTK_STOCK_OPEN), GTK_RESPONSE_ACCEPT, NULL);
	GtkFileFilter *filter = gtk_file_filter_new();

	if (flt != NULL)
	{
		gtk_file_filter_add_pattern(filter, flt);
		gtk_file_filter_set_name(filter, flt);
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	}
	
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{

		*filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}
	
	gtk_widget_destroy(dialog);
}

void set_img_fit()
{
	int tgtw, tgth;
	double wratio, hratio;

	// Thread safe read
	g_rw_lock_reader_lock(&pixbuf_lock);
	if (imgpix_loaded())
	{
		// Get allocated size
		GtkAllocation *alloc = g_new0 (GtkAllocation, 1);
		gtk_widget_get_allocation(GTK_WIDGET(swindow), alloc);
		tgtw = alloc->width;
		tgth = alloc->height;
		//Take scrollbar width into account
		gtk_widget_get_allocation (GTK_WIDGET(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(swindow))), alloc);
		tgtw -= alloc->width; //+ 5;
		gtk_widget_get_allocation (GTK_WIDGET(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(swindow))), alloc);
		tgth -= alloc->height; //+ 10;
		// Cleanup
		g_free(alloc);

		// Resize ratios
		wratio = ((double) imgpix_get_width() / tgtw);
		hratio = ((double) imgpix_get_height() / tgth);
		//printf("w: %d, h: %d, wr: %f, hr: %f\n", tgtw, tgth, wratio, hratio);
		// Choose final size
		if (wratio > hratio)
		{
			tgth = imgpix_get_height() / wratio;
		}
		else
		{
			tgtw = imgpix_get_width() / hratio;
		}	
		//printf("w: %d, h: %d\n", tgtw, tgth);
		// Actual resize
		GdkPixbuf *tmpbuf = gdk_pixbuf_scale_simple(imgpix_get_data(), tgtw, tgth, GDK_INTERP_TILES);
		gtk_image_set_from_pixbuf((GtkImage *) image, tmpbuf);
		g_object_unref(tmpbuf);
	}
	fit = 1;
	g_rw_lock_reader_unlock(&pixbuf_lock);
}

void set_img_full()
{
	// Thread safe read
	g_rw_lock_reader_lock(&pixbuf_lock);
	if (imgpix_loaded())
	{
		gtk_image_set_from_pixbuf((GtkImage *) image, imgpix_get_data());
	}
	fit = 0;
	g_rw_lock_reader_unlock(&pixbuf_lock);
}

void set_adu_limits(int bytepix)
{
	if (uibytepix != bytepix)
	{
		if (bytepix == 2)
		{
			scrmaxadu = gtk_range_get_value(GTK_RANGE(hsc_maxadu)) * 257;
			scrminadu = gtk_range_get_value(GTK_RANGE(hsc_minadu)) * 257;
			gtk_range_set_range(GTK_RANGE(hsc_maxadu), 0.0, 65535.0);
			gtk_range_set_range(GTK_RANGE(hsc_minadu), 0.0, 65534.0);
		}
		else
		{
			scrmaxadu = gtk_range_get_value(GTK_RANGE(hsc_maxadu)) / 257;
			scrminadu = gtk_range_get_value(GTK_RANGE(hsc_minadu)) / 257;
			gtk_range_set_range(GTK_RANGE(hsc_maxadu), 0.0, 255.0);
			gtk_range_set_range(GTK_RANGE(hsc_minadu), 0.0, 254.0);
		}
		gtk_range_set_value(GTK_RANGE(hsc_maxadu), scrmaxadu);
		gtk_range_set_value(GTK_RANGE(hsc_minadu), scrminadu);
		uibytepix = bytepix;
	}
}

void load_image_from_data()
{
	int retval = 0;
	
	g_rw_lock_reader_lock(&thd_caplock);
	if (imgfit_loaded())
	{
		// Set hourglass
		//gdk_window_set_cursor(GDK_WINDOW(window->window), watchCursor);
		// Ui update after byterpix
		if (tmrimgrefresh != -1)
		{
			g_source_remove(tmrimgrefresh);
		}
		if (tmrfrmrefresh != -1)
		{
			g_source_remove(tmrfrmrefresh);
		}
		set_adu_limits(imgfit_get_bytepix());
		
		int debayer = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_debayer));
		if (imgfit_internal() == 0)
		{
			// No debayer for captured frames if bin > 1
			debayer = (imgcam_get_shpar()->bin == 1) ? debayer : 0;
		}

		// Actual pixbuffer load (thread safe)
		g_rw_lock_writer_lock(&pixbuf_lock);
		retval = imgpix_load(imgfit_get_data(), imgfit_get_width(), imgfit_get_height(), imgfit_get_bytepix(), debayer, scrmaxadu, scrminadu);
		g_rw_lock_writer_unlock(&pixbuf_lock);
		
		if (retval == 1)
		{	
			tmrfrmrefresh = g_timeout_add(1, (GSourceFunc) tmr_frm_refresh, NULL);
		}
		else if (strlen(imgpix_get_msg()) != 0)
		{
			gtk_statusbar_write(GTK_STATUSBAR(imgstatus), 0, imgpix_get_msg());
		}
		// Reset
		//gdk_window_set_cursor(GDK_WINDOW(window->window), NULL);
	}
	g_rw_lock_reader_unlock(&thd_caplock);
}

void load_histogram_from_data()
{
	int tgtw, tgth;

	// Thread safe read
	g_rw_lock_reader_lock(&pixbuf_lock);
	if (imgpix_loaded())
	{
		// Get allocated size
		GtkAllocation *alloc = g_new0 (GtkAllocation, 1);
		gtk_widget_get_allocation(GTK_WIDGET(frm_histogram), alloc);
		tgtw = alloc->width -5;
		tgth = alloc->height -5;
		// Cleanup
		g_free(alloc);

		// Actual resize
		#if GTK_MAJOR_VERSION == 3
		tgtw -= 15;
		tgth -= 15;
		#endif
		GdkPixbuf *tmpbuf = gdk_pixbuf_scale_simple(imgpix_get_histogram(gtk_spin_button_get_value(GTK_SPIN_BUTTON(spn_histogram)) * 10), tgtw, tgth, GDK_INTERP_HYPER);
		gtk_image_set_from_pixbuf((GtkImage *) histogram, tmpbuf);
		g_object_unref(tmpbuf);
	}
	g_rw_lock_reader_unlock(&pixbuf_lock);
}

void load_histogram_from_null()
{
	int tgtw, tgth;

	// Thread safe read
	g_rw_lock_reader_lock(&pixbuf_lock);
	if (imgpix_loaded())
	{
		// Get allocated size
		GtkAllocation *alloc = g_new0 (GtkAllocation, 1);
		gtk_widget_get_allocation(GTK_WIDGET(frm_histogram), alloc);
		tgtw = alloc->width -5;
		tgth = alloc->height -5;
		// Cleanup
		g_free(alloc);

		// Actual resize
		imgpix_init_histogram();
		#if GTK_MAJOR_VERSION == 3
		tgtw -= 15;
		tgth -= 15;
		#endif
		GdkPixbuf *tmpbuf = gdk_pixbuf_scale_simple(imgpix_get_data(), tgtw, tgth, GDK_INTERP_HYPER);
		gtk_image_set_from_pixbuf((GtkImage *) histogram, tmpbuf);
		g_object_unref(tmpbuf);
	}
	g_rw_lock_reader_unlock(&pixbuf_lock);
}

void tec_init_graph()
{
	int rowstride;
	int row, col;
	guchar *pixels, *p;
	
	rowstride = gdk_pixbuf_get_rowstride (tecpixbuf);
	pixels    = gdk_pixbuf_get_pixels (tecpixbuf);
	
	for (row = 59; row >= 0; row--)
	{
		for (col = 0; col < 120; col++)
		{
			p = pixels + row * rowstride + col * 3;
			p[0] = 200;
			p[1] = 200;
			p[2] = 200;
		}
	}
	tec_show_graph();
}

void tec_print_graph()
{
	int rowstride;
	int row, col;
	guchar *pixels, *p;
	
	rowstride = gdk_pixbuf_get_rowstride (tecpixbuf);
	pixels    = gdk_pixbuf_get_pixels (tecpixbuf);

	// Shift pixels left one col
	for (row = 59; row >= 0; row--)
	{
		for (col = 0; col < 119; col++)
		{
			p = pixels + row * rowstride + col * 3;
			p[0] = p[3];
			p[1] = p[4];
			p[2] = p[5];
		}
	}
	for (row = 59; row >= 0; row--)
	{
		p = pixels + row * rowstride + 119 * 3;
		p[0] = (imgcam_get_tecp()->tectemp < (((row + 1)*-1) + 20)) ? 200 : 130;
		p[1] = (imgcam_get_tecp()->tectemp < (((row + 1)*-1) + 20)) ? 200 : 160;
		p[2] = 200;
	}	
	tec_show_graph();
}

void tec_show_graph()
{
	int tgtw, tgth;

	// Get allocated size
	GtkAllocation *alloc = g_new0 (GtkAllocation, 1);
	gtk_widget_get_allocation(GTK_WIDGET(frm_tecgraph), alloc);
	tgtw = alloc->width;
	tgth = alloc->height;
	// Cleanup
	g_free(alloc);

	GdkPixbuf *tmpbuf = gdk_pixbuf_scale_simple(tecpixbuf, tgtw, tgth, GDK_INTERP_HYPER);
	gtk_image_set_from_pixbuf((GtkImage *) tecgraph, tmpbuf);
	g_object_unref(tmpbuf);
}

void combo_setlist(GtkWidget *cmb, char *str)
{	
	char tmp[256];
	
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(cmb)) != -1)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(cmb), 0);
		gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(cmb))));
	}
	
	if (strchr(str, '|') != NULL)
	{
		char *pch;
		char *saveptr;
		int val4;
		if (strchr(str, ':') != NULL)
		{
			//Format val1|val2|...:active
			sscanf(str, "%[^:]:%d", tmp, &val4);
			pch = strtok_r(tmp, "|", &saveptr);
			while (pch != NULL)
			{
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmb), pch);
				pch = strtok_r(NULL, "|", &saveptr);
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(cmb), val4);
		}
		else
		{
			// Format val1|val2|...
			pch = strtok_r(str, "|", &saveptr);
			while (pch != NULL)
			{
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmb), pch);
				pch = strtok_r(NULL, "|", &saveptr);
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(cmb), 0);
		}
	}
	else if (strchr(str, ':') != NULL)
	{
		int val1, val2, val3 ,val4, i;
		//Format start:end:step:active
		sscanf(str, "%d:%d:%d:%d", &val1, &val2, &val3, &val4);
		for (i = val1; i <= val2; i+=val3) 
		{
			sprintf(tmp, "%d", i);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmb), tmp);
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(cmb), val4);
	}
}

gpointer thd_capture_run(gpointer thd_data)
{
	int thdrun = 1, thderror = 0, thdhold = 0, thdmode = 0, thdshoot = 0;
	int thdshots = 0, thdpreshots = shots, thdexpnum = 0, thdtimer = 0, thdexp = 0, thdtlmode = 0;
	char thdfit[2048];
	char thdtdmark[32];
	char thdsuffix[32];
	struct tm now;
	time_t localt;
	struct timeval clks, clke;
	GThread *thd_pixbuf = NULL;
	GThread *thd_fitsav = NULL;

	gettimeofday(&clks, NULL);
	g_rw_lock_reader_lock(&thd_caplock);
	thdmode = capture;
	thdrun = run;
	thdtlmode = tlenable + tlcalendar;
	thdexpnum = expnum;
	if ((thdtlmode == 2) && (thdrun == 1))
	{
		// In FULL TimeLapse mode only start / end / interval count
		localt = time(NULL);
		thdrun = ((thdrun == 1) && (difftime(mktime(&tlend), localt) > 0));
	}
	else if ((thdmode == 1) && (thdrun == 1))
	{
		// In capture mode also total shots count
		thdrun = ((thdrun == 1) && (thdexpnum > thdshots));
	}
	g_rw_lock_reader_unlock(&thd_caplock);		

	// If we are in FULL timelapse mode
	if ((thdtlmode == 2) && (thdrun == 1))
	{
		localt = time(NULL);
		// Wait for the start time to arrive
		while (difftime(mktime(&tlstart), localt) > 0)
		{
			usleep(500000);
			// Check if we're still in run condition
			g_rw_lock_reader_lock(&thd_caplock);
			thdrun = run;
			thdmode = capture;
			thdexpnum = expnum;
			g_rw_lock_reader_unlock(&thd_caplock);		
			if (thdrun == 0)
			{
				// User abort
				break;
			}
			localt = time(NULL);
		}
	}
	// We close shutter just in case it's open because of camera position
	// It's a noop for camera that don't feature a mechanical shutter
	imgcam_shutter(1);

	while ((thdrun == 1) && (thderror == 0))
	{
		// To ensure the "sh(oot)" copy of the ex(posure) params is done clean
		g_rw_lock_reader_lock(&thd_caplock);
		thdshoot = imgcam_shoot();
		thdexp = imgcam_get_shpar()->wtime;
		if (thdexp < 1000)
		{
			// Get the time for a complete loop (first loop is invalid result)
			gettimeofday(&clke, NULL);
			fps = 1. / (clke.tv_sec - clks.tv_sec + 0.000001 * (clke.tv_usec - clks.tv_usec));
			gettimeofday(&clks, NULL);
		}
		else
		{
			fps = 0.;
		}
		g_rw_lock_reader_unlock(&thd_caplock);
		if (thdshoot)
		{
			// Ok, exposuring
			g_rw_lock_writer_lock(&thd_caplock);
			readout = 1;
			g_rw_lock_writer_unlock(&thd_caplock);	
			// Active wait for exp time to elapse, unless user abort	
			if (thdexp > 1000)
			{
				thdtimer = 0;
				expfract = 0;
				tmrexpprgrefresh = g_timeout_add(1, (GSourceFunc) tmr_exp_progress_refresh, NULL);
				while (thdtimer < (thdexp))
				{
					usleep(500000);
					thdtimer += 500;
					expfract = (double)thdtimer / (double)thdexp;
					tmrexpprgrefresh = g_timeout_add(1, (GSourceFunc) tmr_exp_progress_refresh, NULL);
					// Check if we're still in run condition
					g_rw_lock_reader_lock(&thd_caplock);
					thdrun = run;
					thdmode = capture;
					thdexpnum = expnum;
					g_rw_lock_reader_unlock(&thd_caplock);		
					if (thdrun == 0)
					{
						// User abort
						g_rw_lock_writer_lock(&thd_caplock);
						readout = 0;
						g_rw_lock_writer_unlock(&thd_caplock);		
						break;
					}
				}
				expfract = 1.0;
			}
			else
			{
				//usleep(thdexp * 1000);
				expfract = -1.0;
			}
			tmrexpprgrefresh = g_timeout_add(1, (GSourceFunc) tmr_exp_progress_refresh, NULL);
			if (thdrun)
			{
				// Unless aborted during exposure time
				if (imgcam_readout())
				{
					//printf("Readout ok\n");
					g_rw_lock_writer_lock(&thd_caplock);
					readout = 0;
					imgfit_init();
					imgfit_set_width(imgcam_get_shpar()->width);
					imgfit_set_height(imgcam_get_shpar()->height);
					imgfit_set_bytepix(imgcam_get_shpar()->bytepix);
					imgfit_set_data(imgcam_get_data());
					g_rw_lock_writer_unlock(&thd_caplock);		
					if (imgfit_loaded())
					{
						if (thdmode == 1)
						{
							// Flags								
							thdshots++;
							
							// Filename calculations
							g_rw_lock_reader_lock(&thd_caplock);
							if ((fitdateadd == 1) || (fittimeadd == 1))
							{
								strcpy(thdfit, g_build_path(G_DIR_SEPARATOR_S, fitfolder, fitbase, NULL));
								localt = time(NULL);
								now    = *(localtime(&localt));
								if (fitdateadd == 1)
								{
									if (strftime(thdtdmark, 32, "_%Y%m%d" ,&now) > 0)
									{
										strcat(thdfit, thdtdmark);
									}
								}
								if (fittimeadd == 1)
								{
									if (strftime(thdtdmark, 32, "_%H%M%S" ,&now) > 0)
									{
										strcat(thdfit, thdtdmark);
									}
								}
								if (strlen(fitflt) > 0)
								{
									sprintf(thdsuffix, "_%s", fitflt);
									strcat(thdfit, thdsuffix);
								}
							}
							else if (audelanaming == 1)
							{
								localt = time(NULL);
								now    = *(localtime(&localt));
								if (strftime(thdtdmark, 32, "_%Y-%m-%d" ,&now) > 0)
								{
									strcpy(thdfit, g_build_path(G_DIR_SEPARATOR_S, fitfolder, thdtdmark+1, G_DIR_SEPARATOR_S, NULL));
									// Check if basefolder exists
									if(isdir(thdfit) == 0)
									{
										// Folders does not exist, create it
										mkpath(thdfit, 0);
									}
									// Base folder/ + date/ + base file name
									strcat(thdfit, fitbase);
									strcat(thdfit, "_");
									strcat(thdfit, imgcam_get_model());
									if (strlen(fitflt) > 0)
									{
										sprintf(thdsuffix, "_%s", fitflt);
										strcat(thdfit, thdsuffix);
									}
									strcat(thdfit, thdtdmark);
									if (strftime(thdtdmark, 32, "_%H-%M-%S" ,&now) > 0)
									{
										strcat(thdfit, thdtdmark);
									}
								}
							}
							else
							{
								strcpy(thdfit, g_build_path(G_DIR_SEPARATOR_S, fitfolder, fitbase, NULL));
								if (strlen(fitflt) > 0)
								{
									sprintf(thdsuffix, "_%s", fitflt);
									strcat(thdfit, thdsuffix);
								}
							}
							if (irisnaming == 1)
							{
								sprintf(thdsuffix, "_%d.fit", thdshots + thdpreshots);
							}
							else	
							{						
								sprintf(thdsuffix, "_%04d.fit", thdshots + thdpreshots);
							}
							strcat(thdfit, thdsuffix);
							strcpy(fitfile, thdfit);
							g_rw_lock_reader_unlock(&thd_caplock);

							g_rw_lock_writer_lock(&thd_caplock);
							shots++;
							thdexpnum = expnum;
							if (thdtlmode == 2)
							{
								// If we are in FULL TimeLapse mode
								localt = time(NULL);
								shotfract = ((double)difftime(localt, mktime(&tlstart)) / (double)difftime(mktime(&tlend), mktime(&tlstart)));
								shotfract = (shotfract > 1.0) ? 1.0 : shotfract;
							}
							else
							{
								shotfract = ((double)thdshots / (double)thdexpnum);
							}
							g_rw_lock_writer_unlock(&thd_caplock);

							if (thd_fitsav != NULL)
							{
								g_thread_unref(thd_fitsav);
								thd_fitsav = NULL;
							}
							// Starts backgroung save of fit file
							thd_fitsav = g_thread_new("Fitsave", thd_fitsav_run, NULL);

							//printf("run: %d, runerr: %d, expnum: %d, shots: %d\n", run, runerr, thdexpnum, thdshots);
						}

						if (thd_pixbuf != NULL)
						{
							// Checks and wait if the pixbuf from previous frame is completed
							g_thread_join(thd_pixbuf);
							thd_pixbuf = NULL;
						}
						// Starts backgroung elaboration of pixbuf
						thd_pixbuf = g_thread_new("Pixbuf", thd_pixbuf_run, NULL);

						//UI update
						if (tmrcapprgrefresh != -1)
						{
							g_source_remove(tmrcapprgrefresh);
						}
						tmrcapprgrefresh = g_timeout_add(1, (GSourceFunc) tmr_capture_progress_refresh, NULL);
					}
				}
			}
			// Determine if loop has to stop
			g_rw_lock_reader_lock(&thd_caplock);
			thdmode = capture;
			thdhold = hold;
			thdrun = run;
			thderror = runerr;
			thdtlmode = tlenable + tlcalendar;
			thdexpnum = expnum;
			if ((thdtlmode == 2) && (thdrun == 1))
			{
				// If we are in FULL TimeLapse mode
				localt = time(NULL);
				thdrun = ((thdrun == 1) && (difftime(mktime(&tlend), localt) > 0));
			}
			else if ((thdmode == 1) && (thdrun == 1))
			{
				thdrun = ((thdrun == 1) && (thdexpnum > thdshots));
			}
			g_rw_lock_reader_unlock(&thd_caplock);
			// If we are in tlmode, even bare tl mode
			if ((thdtlmode > 0) && (thdrun == 1))
			{
				// Wait for the tlperiod seconds in 500ms jumps
				thdtimer = 0;
				while (thdtimer < (tlperiod * 1000))
				{
					usleep(500000);
					thdtimer += 500;
					// Check if we're still in run condition
					g_rw_lock_reader_lock(&thd_caplock);
					thdrun = run;
					thdmode = capture;					
					thdexpnum = expnum;
					g_rw_lock_reader_unlock(&thd_caplock);		
					if (thdrun == 0)
					{
						// User abort
						break;
					}
				}
			}
			// If we are in pause mode
			while ((thdhold == 1) && (thdrun == 1))
			{
				usleep(100000);
				g_rw_lock_reader_lock(&thd_caplock);
				thdhold = hold;
				thdmode = capture;
				thdrun = run;
				thdtlmode = tlenable + tlcalendar;
				thdexpnum = expnum;
				if ((thdtlmode == 2) && (thdrun == 1))
				{
					// If we are in FULL TimeLapse mode
					localt = time(NULL);
					thdrun = ((thdrun == 1) && (difftime(mktime(&tlend), localt) > 0));
				}
				else if ((thdmode == 1) && (thdrun == 1))
				{
					thdrun = ((thdrun == 1) && (thdexpnum > thdshots));
				}
				g_rw_lock_reader_unlock(&thd_caplock);
			}
		}
		else
		{
			thderror = 1;
		}
	}
	g_rw_lock_reader_lock(&thd_caplock);
	thdrun = run;
	g_rw_lock_reader_unlock(&thd_caplock);		
	if ((thdrun > 0) || (thderror == 1))
	{
		g_rw_lock_writer_lock(&thd_caplock);
		run = 0;
		runerr = thderror;
		g_rw_lock_writer_unlock(&thd_caplock);
		// If the thrad ended naturally, ensure the gui is consistent
		if (tmrcapprgrefresh != -1)
		{
			g_source_remove(tmrcapprgrefresh);
		}
		tmrcapprgrefresh = g_timeout_add(1, (GSourceFunc) tmr_capture_progress_refresh, NULL);
	}

	expfract = 0.0;
	tmrexpprgrefresh = g_timeout_add(1, (GSourceFunc) tmr_exp_progress_refresh, NULL);
	
	// It's a noop for camera that don't feature a mechanical shutter
	// Close
	imgcam_shutter(1);
	// Release
	imgcam_shutter(2);
	
	return 0;
}

gpointer thd_pixbuf_run(gpointer thd_data)
{
	// Stop updating anything that may interfere
	if (tmrimgrefresh != -1)
	{
		g_source_remove(tmrimgrefresh);
	}
	if (tmrhstrefresh != -1)
	{
		g_source_remove(tmrhstrefresh);
	}
	// Update image and graph / preview from the main thread (safer)
	load_image_from_data();
	tmrhstrefresh = g_timeout_add(1, (GSourceFunc) tmr_hst_refresh, NULL);
	return 0;
}

gpointer thd_fitsav_run(gpointer thd_data)
{
	// Save fit goes here
	g_rw_lock_reader_lock(&thd_caplock);
	int retval = imgfit_save_file(fitfile);
	g_rw_lock_reader_unlock(&thd_caplock);

	if (retval == 0)
	{
		g_rw_lock_writer_lock(&thd_caplock);
		runerr = 1;
		g_rw_lock_writer_unlock(&thd_caplock);
	}
	return 0;
}

gpointer thd_temp_run(gpointer thd_data)
{
	double mV = 0;
	double oldT;
	int thdrun = 0, setwait = 0, suspect = 0, count = 0;

	g_rw_lock_reader_lock(&thd_teclock);
	oldT = imgcam_get_tecp()->tectemp;
	thdrun = tecrun;
	g_rw_lock_reader_unlock(&thd_teclock);
	// This tread locks the mutex unless it's in the 5s pause (timer)
	// Therefore other threads have this time window to change the thread behavior using the public variables
	while (thdrun == 1)
	{
		// This will wait 10 * half a second.
		// checking for user interrupt.
		count = 0;
		while ((thdrun == 1) && (count < 10))
		{
			count++;
			usleep(500000);
			g_rw_lock_reader_lock(&thd_teclock);
			thdrun = tecrun;
			g_rw_lock_reader_unlock(&thd_teclock);
		}
		if (thdrun == 1)
		{
			// Not user interrupted
			g_rw_lock_writer_lock(&thd_teclock);
			if (imgcam_gettec(&imgcam_get_tecp()->tectemp, &mV))
			{
				if (imgcam_get_tecp()->tecauto)
				{
					if (setwait == 0)
					{
						if (fabs(oldT - imgcam_get_tecp()->tectemp) == 0 && suspect < 3)
						{
							//This is suspect... noop
							suspect++;
						}
						else if (fabs(imgcam_get_tecp()->tectemp - imgcam_get_tecp()->settemp) < 2.)
						{
							suspect = 0;
							// Temp is almost stabilized near to target, we do tiny corrections
							if ((oldT - imgcam_get_tecp()->tectemp) < 0.03 && imgcam_get_tecp()->tectemp > imgcam_get_tecp()->settemp)
							{
								// If temp is not moving or not the right direction
								if (imgcam_get_tecp()->tectemp > (imgcam_get_tecp()->settemp + 0.7) ) 
								{
									imgcam_get_tecp()->tecpwr += 2;
									imgcam_get_tecp()->tecpwr = MIN(imgcam_get_tecp()->tecpwr, imgcam_get_tecp()->tecmax);
									if (imgcam_get_tecp()->tectemp > oldT)
									{
										//Still going wrong direction
										setwait = 3;
									}
									else
									{
										setwait = 6;
									}
								}
								else if (imgcam_get_tecp()->tectemp > (imgcam_get_tecp()->settemp + 0.2)) 
								{
									imgcam_get_tecp()->tecpwr += 1;
									imgcam_get_tecp()->tecpwr = MIN(imgcam_get_tecp()->tecpwr, imgcam_get_tecp()->tecmax);
									if (imgcam_get_tecp()->tectemp > oldT)
									{
										//Still going wrong direction
										setwait = 1;
									}
									else
									{
										setwait = 3;
									}
								}
							}
							else if ((imgcam_get_tecp()->tectemp - oldT) < 0.03 && imgcam_get_tecp()->tectemp < imgcam_get_tecp()->settemp)
							{
								// If temp is not moving or not the right direction
								if (imgcam_get_tecp()->tectemp < (imgcam_get_tecp()->settemp - 0.7) ) 
								{
									imgcam_get_tecp()->tecpwr -= 2;
									imgcam_get_tecp()->tecpwr = MAX(imgcam_get_tecp()->tecpwr, 0);
									if (imgcam_get_tecp()->tectemp < oldT)
									{
										//Still going wrong direction
										setwait = 3;
									}
									else
									{
										setwait = 6;
									}
								}
								else if (imgcam_get_tecp()->tectemp < (imgcam_get_tecp()->settemp - 0.2)) 
								{
									imgcam_get_tecp()->tecpwr -= 1;
									imgcam_get_tecp()->tecpwr = MAX(imgcam_get_tecp()->tecpwr, 0);
									if (imgcam_get_tecp()->tectemp < oldT)
									{
										//Still going wrong direction
										setwait = 1;
									}
									else
									{
										setwait = 3;
									}
								}
							}
						}
						else if (imgcam_get_tecp()->settemp < imgcam_get_tecp()->tectemp) 
						{
							suspect = 0;
							if ((oldT - imgcam_get_tecp()->tectemp) < 0.06)
							{
								//setTemp is still far. We gently pull tec up or down
								imgcam_get_tecp()->tecpwr += 6;
								imgcam_get_tecp()->tecpwr = MIN(imgcam_get_tecp()->tecpwr, imgcam_get_tecp()->tecmax);
								if (imgcam_get_tecp()->tectemp > oldT)
								{
									//Still going wrong direction
									setwait = 1;
								}
								else
								{
									setwait = 3;
								}
							}
						}
						else if (imgcam_get_tecp()->settemp > imgcam_get_tecp()->tectemp) 
						{
							suspect = 0;
							if ((imgcam_get_tecp()->tectemp - oldT) < 0.06)
							{
								imgcam_get_tecp()->tecpwr -= 6;
								imgcam_get_tecp()->tecpwr = MAX(imgcam_get_tecp()->tecpwr, 0);
								if (imgcam_get_tecp()->tectemp < oldT)
								{
									//Still going wrong direction
									setwait = 1;
								}
								else
								{
									setwait = 3;
								}
							}
						}
						if (setwait)
						{
							imgcam_settec(imgcam_get_tecp()->tecpwr);
						}
					}
					else
					{
						setwait--;
					}
				}
				//Set the loop reference
				oldT = imgcam_get_tecp()->tectemp;
			}
			g_rw_lock_writer_unlock(&thd_teclock);
		}
		// UI update
		if (tmrtecrefresh != -1)
		{
			g_source_remove(tmrtecrefresh);
		}
		tmrtecrefresh = g_timeout_add(1, (GSourceFunc) tmr_tecstatus_write, NULL);			
	}
	return 0;
}
