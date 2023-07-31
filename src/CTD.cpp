#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CTD.h"


#define LONGEST_CTD_STR "ttt.tttt, cc.ccccc, pppp.ppp, sss.ssss, vvvv.vvv\n"


// Modify depending on the configuration settings on the CTD
#define OUTPUT_SAL 1
#define OUTPUT_SV  0


// How many samples to accumulate before outputting an averaged one. The SBE 49
// takes samples 16 Hz, so this causes output at 1 Hz.
#define MAX_SAMPLES 16


static struct {
    float temperature;
    float conductivity;
    float pressure;
#if OUTPUT_SAL
    float salinity;
#endif
#if OUTPUT_SV
    float sound_velocity;
#endif
} samples[MAX_SAMPLES];


static int n_samples = 0;


// Input from serial is accumulated here until we get a full line
static char rx_buffer[2*sizeof(LONGEST_CTD_STR)] = {0};
static int rx_buffer_used = 0;
#define rx_buffer_available (sizeof(rx_buffer) - rx_buffer_used)


static void append_rx(const char *input, size_t len) {
    /* Here be dragons. */

    if (len == 0)
        return;

    // If not enough space is available, shift buffer contents left
    if (rx_buffer_available < len) {
        size_t needed = (len <= sizeof(rx_buffer))
            ? (len - rx_buffer_available) : sizeof(rx_buffer);
        memmove(rx_buffer, rx_buffer + needed, sizeof(rx_buffer) - needed);
        rx_buffer_used -= needed;
    }

    // Copy as much of the input buffer as we can fit
    if (len <= rx_buffer_available) {
        memmove(rx_buffer + rx_buffer_used, input, len);
        rx_buffer_used += len;
    } else {
        memmove(rx_buffer + rx_buffer_used, input + len - rx_buffer_available,
            rx_buffer_available);
        rx_buffer_used = sizeof(rx_buffer);
    }
}


void handle_ctd_input(writefn_t writefn, char *input, size_t len) {
    // Append the input to the receive buffer
    append_rx(input, len);

    // Scan the receive buffer for a newline. If found, we have a full input
    // line we can parse.
    char *eol = strchr(rx_buffer, '\n');
    if (eol == NULL) {
        return;
    }
    *eol = '\0';

    // Now our buffer contains a null-terminated line of CTD data.
    //
    // Parse the fields from it.
    char *buf_ptr = rx_buffer;
    samples[n_samples].temperature = atof(strsep(&buf_ptr, ", "));
    samples[n_samples].conductivity = atof(strsep(&buf_ptr, ", "));
    samples[n_samples].pressure = atof(strsep(&buf_ptr, ", "));
#if OUTPUT_SAL
    samples[n_samples].salinity = atof(strsep(&buf_ptr, ", "));
#endif
#if OUTPUT_SV
    samples[n_samples].sound_velocity = atof(strsep(&buf_ptr, ", "));
#endif
    n_samples ++;

    if (n_samples == MAX_SAMPLES) {
        // Sum all samples into the first slot
        for (int i = 1; i < MAX_SAMPLES; i ++) {
            samples[0].temperature += samples[i].temperature;
            samples[0].conductivity += samples[i].conductivity;
            samples[0].pressure += samples[i].pressure;
#if OUTPUT_SAL
            samples[0].salinity += samples[i].salinity;
#endif
#if OUTPUT_SV
            samples[0].sound_velocity += samples[i].sound_velocity;
#endif
        }

        // Compute the average of the samples
        samples[0].temperature /= MAX_SAMPLES;
        samples[0].conductivity /= MAX_SAMPLES;
        samples[0].pressure /= MAX_SAMPLES;
#if OUTPUT_SAL
        samples[0].salinity /= MAX_SAMPLES;
#endif
#if OUTPUT_SV
        samples[0].sound_velocity /= MAX_SAMPLES;
#endif

        // Output the average
        char buffer[strlen(LONGEST_CTD_STR) + 1];
        snprintf(
            buffer,
            strlen(LONGEST_CTD_STR) + 1,
            "%8.4f, %8.5f, %8.3f"
#if OUTPUT_SAL
            ", %8.4f"
#endif
#if OUTPUT_SV
            ", %8.3f"
#endif
            "\n",
            samples[0].temperature,
            samples[0].conductivity,
            samples[0].pressure
#if OUTPUT_SAL
            , samples[0].salinity
#endif
#if OUTPUT_SV
            , samples[0].sound_velocity
#endif
        );
        writefn(buffer);

        // Reset the insertion cursor
        n_samples = 0;
    }

    // Lastly, shift the receive left to consume the line. Now the buffer begins
    // with the byte to the right of the newline.
    rx_buffer_used -= (eol + 1 - rx_buffer);
    memmove(rx_buffer, eol + 1, rx_buffer_used);
}
