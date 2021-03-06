/*
* main.h : defines FFT_Frame struct, PI
*
*
*
* Copyright (C) 2007-2008 Brian Phelps(lm317t@gmail.com)
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Library General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <fftw3.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <math.h>
#include <stdlib.h>
#define pi 3.141592653

#ifndef MAIN_H
#define MAIN_H

#define c_re(c) ((c)[0])
#define c_im(c) ((c)[1])

struct FFT_Frame {
    fftw_plan plan1;              // FFT Plan
    fftw_plan plan2;              // FFT Plan
    fftw_plan reverse_plan;      // FFT Plan
    
    fftw_complex *plan_buf1;
    fftw_complex *plan_buf2;

    short *prewin_buffer_data_1; // Data from channel
    short *prewin_buffer_data_2;
    short *buffer_data_1; // data from channel after window function
    short *buffer_data_2; // data from channel after window function
    double *fft_returned_1;
    double *fft_returned_2;
    double *rfft_returned_1;
    float volume_pink;
    char pink_muted;
    //////////////////
    short *delay;
    int delay_size;
    char find_delay;
    char find_impulse;
};

#endif
