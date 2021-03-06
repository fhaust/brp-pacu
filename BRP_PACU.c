/*
* BRP_PACU.c : Contains main, calls creation of the gui, initialization of
*       main FFT data struct and its array variables, creates the audio capture
*       thread, creates the 100ms gui callback, handles the gui callback and
*       on quitting tries to free all allocated memory
*
* Some of this should probably moved out of this huge c file and organized
*       better.
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

#include "gui.h"
#include "main.h"
#include "testfft.h"
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <jack/jack.h>
#include <string.h>

volatile struct FFT_Frame *fill_it;
volatile struct FFT_Frame
    *temp_frame_data; // tmp copy so mutexes don't need to wait
static float audio1[8192];
static float audio2[8192];
static float pink_noise[8192];
volatile char run = 1;
static guint timer_id = 0;
// static guint BUF_SIZE = BUFSIZE;
float b0, b1, b2, b3, b4, b5, b6, white;
float tmp_time = 0.0;
float scale_it = 0.98;
float volume = 0.5;
GMutex *thread_mutex;

jack_port_t *measured_input_port;
jack_port_t *reference_input_port;
jack_port_t *generator_output_port;
jack_client_t *client;

/* a simple state machine for this client */
volatile enum { Init, Run, Started, Exit } client_state = Init;

/*static void
 on_quit (GtkObject *object)
{
   gtk_object_destroy (object);
   gtk_main_quit ();
}*/

int Fill_Buffer(jack_nframes_t nframes, void *arg) {
    // fill_it = (struct FFT_Frame *) malloc(sizeof(struct FFT_Frame ));
    int k, j, period_size;
    jack_default_audio_sample_t *in_buffer1, *in_buffer2, *out_buffer;

    g_mutex_lock(thread_mutex);
    // if (pthread_mutex_trylock(&p_mutex))
    {
        jack_transport_state_t ts = jack_transport_query(client, NULL);

        if (ts == JackTransportRolling) {
            if (client_state == Init)
                client_state = Run;

            in_buffer1 = jack_port_get_buffer(measured_input_port, nframes);
            in_buffer2 = jack_port_get_buffer(reference_input_port, nframes);
            out_buffer = jack_port_get_buffer(generator_output_port, nframes);

            for (k = 0; k < nframes; k++) {
                white = ((float)(rand() % 10000) - 10000.0 / 2.0) / 999900.0;
                // Shamelessly taken from
                // http://www.firstpr.com.au/dsp/pink-noise/
                // Based on a filter by  Robert Bristow-Johnson
                b0 = (0.99886 * b0 + white * 0.0555179) * scale_it;
                b1 = (0.99332 * b1 + white * 0.0750759) * scale_it;
                b2 = (0.96900 * b2 + white * 0.1538520) * scale_it;
                b3 = (0.86650 * b3 + white * 0.3104856) * scale_it;
                b4 = (0.55000 * b4 + white * 0.5329522) * scale_it;
                b5 = (-0.7616 * b5 - white * 0.0168980) * scale_it;
                tmp_time = tmp_time + 1.0 / 44100;
                if (fill_it->pink_muted)
                    pink_noise[k] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 +
                                     (white * 0.5362) * scale_it) *
                                    (pow(2.0, fill_it->volume_pink + 29) /
                                     (pow(2.0, 32.0)));
                else
                    pink_noise[k] = 0.0;
                b6 = (white * 0.115926) * scale_it;
            }

            memcpy(audio1, in_buffer1,
                   sizeof(jack_default_audio_sample_t) * nframes);
            memcpy(audio2, in_buffer2,
                   sizeof(jack_default_audio_sample_t) * nframes);
            memcpy(out_buffer, pink_noise,
                   sizeof(jack_default_audio_sample_t) * nframes);

        } else if (ts == JackTransportStopped) {
            if (client_state == Run)
                client_state = Exit;
            jack_transport_start(client);
        }

        period_size = nframes;
        // fprintf(stderr, "The period size is %d\n",period_size);
        //
        // Fill delay with old data
        for (k = 0; k < (DELAY_BUFFER_SIZE - period_size); k++) {
            // Rotate dellay data to the left to make room for new
            // samples
            fill_it->delay[k] = fill_it->delay[k + period_size];
        }

        j = N_FFT - period_size;
        for (k = DELAY_BUFFER_SIZE - period_size; k < DELAY_BUFFER_SIZE; k++) {
            //// Copy old delay data to end of delayed buffer 2
            // Copy old buffer data to end of delay buffer
            fill_it->delay[k] =
                fill_it->prewin_buffer_data_2[j - N_FFT + period_size];
            j++;
        }

        // Rotate data to the left to make room for new samples
        for (k = 0; k < (N_FFT - period_size); k++) {
            fill_it->prewin_buffer_data_1[k] =
                fill_it->prewin_buffer_data_1[k + period_size];
            fill_it->prewin_buffer_data_2[k] =
                fill_it->prewin_buffer_data_2[k + period_size];
        }

        j = 0;
        for (k = N_FFT - period_size; k < N_FFT; k++) {
            // copy channels to the end of the data

            fill_it->prewin_buffer_data_1[k] =
                (short)32767.0 * audio1[j]; // Copy Begining of
                                            // Audio buff to end of
                                            // delay from last
                                            // buffer fill
            fill_it->prewin_buffer_data_2[k] = (short)32767.0 * audio2[j];
            j++;
        }

        for (k = 0; k < N_FFT; k++) {
            // Copy data to working buffer #2
            fill_it->buffer_data_1[k] = fill_it->prewin_buffer_data_1[k];
            // Copy data to working #1 and apply delay
            if (k - fill_it->delay_size >= 0)
                fill_it->buffer_data_2[k] =
                    fill_it
                        ->prewin_buffer_data_2[k -
                                               fill_it->delay_size]; // copy end
            // of normal
            // buffer to
            // working
            // buffer
            else
                fill_it->buffer_data_2[k] =
                    fill_it->delay[k + DELAY_BUFFER_SIZE -
                                   fill_it->delay_size]; // copy
                                                         // most
                                                         // recent samples
                                                         // of the delay
                                                         // buffer to
                                                         // beginning
                                                         // (oldest)
                                                         // buffer_data_2
        }
        // for (k = 0; k < N_FFT; k++)
        //{
        // Apply Window function
        // Blackman window
        // For some reason this causes errors in the response, maybe
        // someone knows better than me why?
        // fill_it->buffer_data_1[k] = (short)(
        //      ( ((float)fill_it->buffer_data_1[k])*(0.42 - 0.5 *
        //      cos(2.0 * pi * ((float)k) / (  ((float)N) - 1.0))
        //        + 0.08 * cos(4.0 * pi * ((float)k) / (((float)N) -
        //        1.0)))  ));
        // fill_it->buffer_data_2[k] = (short)(
        //   ((float)fill_it->buffer_data_2[k])*(0.42 - 0.5 * cos(2.0 *
        //   pi * ((float)k) / (  ((float)N) - 1.0))
        //    + 0.08 * cos(4.0 * pi * ((float)k) / (((float)N) -
        //    1.0))));
        //
        //}
    }
    g_mutex_unlock(thread_mutex);
    return 0;
}

static gboolean MyGTKFunction(struct FFT_Frame *frame_data) {
    int j, max_index = 0;
    //   float avg[2000], min;
    double max, tmp;

    g_mutex_lock(thread_mutex);
    memcpy(temp_frame_data->buffer_data_1, frame_data->buffer_data_1,
           N_FFT * sizeof(short));
    memcpy(temp_frame_data->buffer_data_2, frame_data->buffer_data_2,
           N_FFT * sizeof(short));
    memcpy(temp_frame_data->plan1, frame_data->plan1, sizeof(frame_data->plan1));
    memcpy(temp_frame_data->plan2, frame_data->plan2, sizeof(frame_data->plan2));
    g_mutex_unlock(thread_mutex);
    fft_capture((struct FFT_Frame *)temp_frame_data); // This fxn does the FFT,
    // frame_data->buffer_data_n
    // is used to get
    // frame_data->fft_returned_n

    g_mutex_lock(thread_mutex);
    memcpy(frame_data->fft_returned_1, temp_frame_data->fft_returned_1,
           N_FFT * sizeof(double));
    memcpy(frame_data->fft_returned_2, temp_frame_data->fft_returned_2,
           N_FFT * sizeof(double));
    g_mutex_unlock(thread_mutex);

    if (frame_data->find_delay ==
        1) // Delay button pressed, set delay to new value
    {
        g_source_remove(timer_id);
        gui_idle_func(frame_data);
        frame_data->delay_size = 0;
        frame_data->find_delay = 3; // Signal to gui that we are waiting
                                    // while the delay sets in
        timer_id =
            g_timeout_add(1000, (GSourceFunc)MyGTKFunction,
                          (gpointer)frame_data); // Start the initial delay
        printf("Finding delay....\n");
        return TRUE;
    } else if (frame_data->find_delay == 3) {
        g_source_remove(timer_id);

        g_mutex_lock(thread_mutex);
        impulse_capture(frame_data);
        g_mutex_unlock(thread_mutex);
        max = 0.0;
        for (j = 0; j < N_FFT; j++) {
            tmp = frame_data->rfft_returned_1[j];
            // find the maximum impulse response sample to find the
            // delay.
            if (max < (tmp * tmp)) {
                max = (tmp * tmp);
                max_index = j;
            }
            // printf("tmp:%f max:%f j:%d max_index:%d\n ", tmp,
            // max, j, max_index);
        }

        printf("Done Finding delay\n");
        printf("min value is %f, min time is %f\n", max,
               ((float)max_index) * (1.0 / ((float)FSAMP)));
        // frame_data->delay_size=10000;
        if (max_index < N_FFT) // Protect against delay being larger
                               // than malloc'd size of N
            frame_data->delay_size = max_index;
        timer_id = g_timeout_add(90, (GSourceFunc)MyGTKFunction,
                                 (gpointer)frame_data); // Start back to normal
        frame_data->find_delay =
            2; // Indicate to gui that we are done finding the delay
        if (frame_data->find_impulse == 1) {
            frame_data->find_impulse = 2; // Signal to gui that
                                          // impulse response has
                                          // been done
        }
    }

    g_mutex_lock(thread_mutex);
    temp_frame_data->pink_muted = frame_data->pink_muted;
    temp_frame_data->volume_pink = frame_data->volume_pink;
    temp_frame_data->find_delay = frame_data->find_delay;
    temp_frame_data->find_impulse = frame_data->find_impulse;
    temp_frame_data->delay_size = frame_data->delay_size;
    //   memcpy(temp_frame_data->delay_size, frame_data->delay,
    //   N_FFT*sizeof(short));
    memcpy(temp_frame_data->fft_returned_1, frame_data->fft_returned_1,
           N_FFT * sizeof(double));
    memcpy(temp_frame_data->fft_returned_2, frame_data->fft_returned_2,
           N_FFT * sizeof(double));
    memcpy(temp_frame_data->buffer_data_1, frame_data->buffer_data_1,
           N_FFT * sizeof(short));
    memcpy(temp_frame_data->buffer_data_2, frame_data->buffer_data_2,
           N_FFT * sizeof(short));
    memcpy(temp_frame_data->rfft_returned_1, frame_data->rfft_returned_1,
           N_FFT * sizeof(double));
    g_mutex_unlock(thread_mutex);

    gui_idle_func((struct FFT_Frame *)temp_frame_data);

    g_mutex_lock(thread_mutex);
    frame_data->pink_muted = temp_frame_data->pink_muted;
    frame_data->volume_pink = temp_frame_data->volume_pink;
    frame_data->find_delay = temp_frame_data->find_delay;
    frame_data->find_impulse = temp_frame_data->find_impulse;
    frame_data->delay_size = temp_frame_data->delay_size;
    g_mutex_unlock(thread_mutex);

    return TRUE;
}

struct FFT_Frame *init_fft_frame(void) {
    int k;
    struct FFT_Frame *FFT_Kit =
        (struct FFT_Frame *)malloc(sizeof(struct FFT_Frame));
    // p_mutex = PTHREAD_MUTEX_INITIALIZER;

    FFT_Kit->fft_returned_1 = (double *)malloc(sizeof(double) * N_FFT);
    FFT_Kit->fft_returned_2 = (double *)malloc(sizeof(double) * N_FFT);
    FFT_Kit->rfft_returned_1 = (double *)malloc(sizeof(double) * N_FFT);
    FFT_Kit->prewin_buffer_data_1 = (short *)malloc(sizeof(short) * N_FFT);
    FFT_Kit->prewin_buffer_data_2 = (short *)malloc(sizeof(short) * N_FFT);
    FFT_Kit->buffer_data_1 = (short *)malloc(sizeof(short) * N_FFT);
    FFT_Kit->buffer_data_2 = (short *)malloc(sizeof(short) * N_FFT);
    FFT_Kit->delay = (short *)malloc(sizeof(short) * DELAY_BUFFER_SIZE);

    FFT_Kit->delay_size = 0;
    FFT_Kit->volume_pink = 0.0;
    FFT_Kit->find_delay = 0;
    FFT_Kit->find_impulse = 0;

    for (k = 0; k < N_FFT; k++) {
        FFT_Kit->delay[k] = 0;
        // FFT_Kit->audio[k] = 0;
        FFT_Kit->prewin_buffer_data_1[k] = 0;
        FFT_Kit->prewin_buffer_data_2[k] = 0;
        FFT_Kit->buffer_data_1[k] = 0;
        FFT_Kit->buffer_data_2[k] = 0;
        FFT_Kit->fft_returned_1[k] = 0;
        FFT_Kit->fft_returned_2[k] = 0;
        FFT_Kit->delay[k] = 0;
    }

    FFT_Kit->plan_buf1 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N_FFT);
    FFT_Kit->plan_buf2 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N_FFT);

    FFT_Kit->plan1 = fftw_plan_dft_1d(N_FFT, FFT_Kit->plan_buf1, FFT_Kit->plan_buf1, FFTW_FORWARD, FFTW_ESTIMATE);
    FFT_Kit->plan2 = fftw_plan_dft_1d(N_FFT, FFT_Kit->plan_buf2, FFT_Kit->plan_buf2, FFTW_FORWARD, FFTW_ESTIMATE);
    FFT_Kit->reverse_plan = fftw_plan_dft_1d(N_FFT, FFT_Kit->plan_buf1, FFT_Kit->plan_buf1, FFTW_BACKWARD, FFTW_ESTIMATE);
    return FFT_Kit;
}

void jack_shutdown(void *arg) {
    fprintf(stderr, "Jack shutdown!  Exiting..\n");
    gtk_main_quit();
}
int jack_init() {
    const char **ports;
    char *client_name = "BRP_PACU";
    const char *server_name = NULL;
    jack_options_t options = JackNullOption;
    jack_status_t status;

    /* open a client connection to the JACK server */

    client = jack_client_open(client_name, options, &status, server_name);
    if (client == NULL) {
        fprintf(stderr, "jack_client_open() failed, "
                        "status = 0x%2.0x\n",
                status);
        if (status & JackServerFailed) {
            fprintf(stderr, "Unable to connect to JACK server\n");
        }
        return 1;
    }
    if (status & JackServerStarted) {
        fprintf(stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        client_name = jack_get_client_name(client);
        fprintf(stderr, "unique name `%s' assigned\n", client_name);
    }

    /* tell the JACK server to call `process()' whenever
       there is work to be done.
    */

    jack_set_process_callback(client, Fill_Buffer, 0);

    /* tell the JACK server to call `jack_shutdown()' if
       it ever shuts down, either entirely, or if it
       just decides to stop calling us.
    */

    jack_on_shutdown(client, jack_shutdown, 0);

    /* display the current sample rate.
     */

    printf("engine sample rate: %d\n", jack_get_sample_rate(client));

    /* create two ports */

    measured_input_port = jack_port_register(client, "Measured Signal", JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsInput, 0);
    reference_input_port = jack_port_register(client, "Reference Signal", JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsInput, 0);
    generator_output_port = jack_port_register(client, "Generator Out", JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsOutput, 0);

    if ((measured_input_port == NULL) || (reference_input_port == NULL) ||
        (generator_output_port == NULL)) {
        fprintf(stderr, "no more JACK ports available\n");
        return 2;
    }

    /* Tell the JACK server that we are ready to roll.  Our
     * process() callback will start running now. */

    if (jack_activate(client)) {
        fprintf(stderr, "cannot activate client");
        return 3;
    }

    /* Connect the ports.  You can't do this before the client is
     * activated, because we can't make connections to clients
     * that aren't running.  Note the confusing (but necessary)
     * orientation of the driver backend ports: playback ports are
     * "input" to the backend, and capture ports are "output" from
     * it.
     */

    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical | JackPortIsOutput);
    if (ports == NULL) {
        fprintf(stderr, "no physical capture ports\n");
        return 4;
    }

    if (jack_connect(client, ports[0], jack_port_name(measured_input_port))) {
        fprintf(stderr, "cannot connect input ports\n");
        return 6;
    }
    if (jack_connect(client, ports[1], jack_port_name(reference_input_port))) {
        fprintf(stderr, "cannot connect input ports\n");
        return 6;
    }
    // Connect the Pink Noise Output to the reference input.
    if (jack_connect(client, jack_port_name(generator_output_port),
                     jack_port_name(reference_input_port))) {
        fprintf(stderr, "cannot connect output ports\n");
        return 7;
    }

    free(ports);

    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical | JackPortIsInput);
    if (ports == NULL) {
        fprintf(stderr, "no physical playback ports\n");
        return 5;
    }
    // Pink Noise Output
    if (jack_connect(client, jack_port_name(generator_output_port), ports[1])) {
        fprintf(stderr, "cannot connect output ports\n");
        return 7;
    }

    free(ports);

    /* keep running until the transport stops */
    return 0;
}

int main(int argc, char *argv[]) {
    //        struct FFT_Frame *FFT_Kit = g_new0 (struct FFT_Frame, 1);

    b0 = 0;
    b1 = 0;
    b2 = 0;
    b3 = 0;
    b4 = 0;
    b5 = 0;
    b6 = 0;
    white = 0;
    printf("BLAH\n");
    gtk_init(&argc, &argv);
    int ierr = -1;

    thread_mutex = (GMutex *) malloc(sizeof(GMutex));
    g_mutex_init(thread_mutex);

    char *jackErrMessage[8] = {"OK",
                               "Unable to connect to JACK server",
                               "No more JACK ports available",
                               "Cannot activate client",
                               "No physical capture ports",
                               "No physical playback ports",
                               "Cannot connect input ports",
                               "Cannot connect output ports"};

    fill_it = init_fft_frame();
    temp_frame_data = init_fft_frame();

    GtkWidget *jack_error_dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
        GTK_BUTTONS_NONE, "Jack initialization Error");
    gtk_window_set_decorated(GTK_WINDOW(jack_error_dialog), FALSE);
    gtk_window_set_position(GTK_WINDOW(jack_error_dialog), GTK_WIN_POS_CENTER);
    gtk_dialog_add_buttons(GTK_DIALOG(jack_error_dialog), "Continue", 1, "Quit",
                           2, NULL);
    while (ierr != 0) {
        ierr = jack_init();
        if (ierr == 0)
            break; // If jack init, is successful, run GUI
        if (client != NULL)
            jack_client_close(client);
        fprintf(stderr, "////////\n BRP_PACU failed to start because "
                        "jackd failed to initialize\n\n%s\n\nplease "
                        "check your jackd sound card settings.  "
                        "QjackCtl (JackPilot with a mac) is an easy "
                        "way to do this\n////////\n",
                jackErrMessage[ierr]);
#ifdef __APPLE__
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(jack_error_dialog),
            "BRP_PACU failed to start.\n-- %s --\nCheck the settings "
            "of Audio MIDI Setup and JackPilot, then click on "
            "\"Continue\"",
            jackErrMessage[ierr]);
        system("open -b gpl.elementicaotici.JackPilot"); // Launch
                                                         // JackPilot so
                                                         // that the
                                                         // user has not
                                                         // to search
                                                         // around
#else
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(jack_error_dialog),
            "BRP_PACU failed to start.\n-- %s --\nCheck the settings "
            "of QjackCtl, then click on \"Continue\"",
            jackErrMessage[ierr]);
#endif
        if (gtk_dialog_run(GTK_DIALOG(jack_error_dialog)) ==
            2) { // user clicked on "Quit"
            gtk_widget_destroy(GTK_WIDGET(jack_error_dialog));
            printf("BRP-PACU terminated by user. Thank you for "
                   "using BRP-PACU\n");
            return (64 + ierr); // exit program with non-zero exit status
        };
    }
    gtk_widget_destroy(GTK_WIDGET(jack_error_dialog));
    if (create_gui((struct FFT_Frame *)fill_it, DATADIR)) {
        timer_id = g_timeout_add(90, (GSourceFunc)MyGTKFunction,
                                 (gpointer)fill_it); // Start the initial delay
        gtk_main();

        g_source_remove(timer_id);
        run = 0;
    } else
        fprintf(stderr, "Gui did not start\n");

    // Wait until thread execution has ended
    printf("Main Cleaning up.......\n");
    jack_deactivate(client);
    jack_client_close(client);
    fftw_destroy_plan(fill_it->plan1);
    fftw_destroy_plan(fill_it->plan2);
    fftw_destroy_plan(fill_it->reverse_plan);
    free(fill_it->delay);
    free(fill_it->buffer_data_1);
    free(fill_it->buffer_data_2);
    free(fill_it->prewin_buffer_data_1);
    free(fill_it->prewin_buffer_data_2);
    free(fill_it->fft_returned_1);
    free(fill_it->rfft_returned_1);
    free(fill_it->fft_returned_2);
    free((struct FFT_Frame *)fill_it);
    printf("Main has exited. Thank you for using BRP-PACU\n");
    return 0;
}
