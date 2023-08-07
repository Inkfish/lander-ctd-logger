#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CTD.h"


#define LONGEST_CTD_STR "ttt.tttt, cc.ccccc, pppp.ppp, sss.ssss, vvvv.vvv\n"


// How many samples to accumulate before outputting an averaged one. The SBE 49
// takes samples 16 Hz, so this causes output at 1 Hz.
#define MAX_SAMPLES 16


static struct {
    float temperature;
    float conductivity;
    float pressure;
    float salinity;
    float sound_velocity;
} samples[MAX_SAMPLES];


static int n_samples = 0;


// Ring buffer holding data that we receive
struct {
    char buffer[sizeof(LONGEST_CTD_STR)];
    char *head = NULL;
    char *tail = NULL;
    int full = 0;
} rx_buffer;

static void rxb_write(char value) {
    if (rx_buffer.full)
        return;

    *(rx_buffer.head++) = value;

    // Wrap head around when it hits the bounds of the buffer
    if (rx_buffer.head == rx_buffer.buffer + sizeof(rx_buffer.buffer))
        rx_buffer.head = rx_buffer.buffer;

    // When head catches up to tail, the buffer is full
    if (rx_buffer.head == rx_buffer.tail)
        rx_buffer.full = 1;
}


// Pop up to one byte from the queue. Return 1 on success, 0 on failure.
static size_t rxb_read(char *value) {
    // If head == tail the buffer is either full or empty. If empty, return -1.
    if (rx_buffer.head == rx_buffer.tail && !rx_buffer.full)
        return 0;

    *value = *(rx_buffer.tail++);

    // Wrap tail around when it hits the bounds of the buffer
    if (rx_buffer.tail == rx_buffer.buffer + sizeof(rx_buffer.buffer))
        rx_buffer.tail = rx_buffer.buffer;

    // After reading we're not full anymore
    rx_buffer.full = 0;

    return 1;
}


// Read all the bytes into a contiguous output buffer. The output buffer must
// be the correct size. Return the number of bytes read.
static size_t rb_read_all(char *output) {
    // Edge case: buffer is empty
    if (rx_buffer.head == rx_buffer.tail && !rx_buffer.full)
        return 0;

    size_t count = 0;

    // If head is after tail, the contents are already contiguous
    if (rx_buffer.head > rx_buffer.tail) {
        count = rx_buffer.head - rx_buffer.tail;
        memcpy(output, rx_buffer.tail, count);

    // Otherwise, we have two spans to copy: from tail to the end of the buffer,
    // then from the start of the buffer to head.
    } else {
        count = sizeof(rx_buffer.buffer) - (rx_buffer.tail - rx_buffer.buffer);
        memcpy(output, rx_buffer.tail, count);
        memcpy(output + count, rx_buffer.buffer,
            rx_buffer.head - rx_buffer.buffer);
        count += rx_buffer.head - rx_buffer.buffer;
    }

    // Drain the buffer
    rx_buffer.tail = rx_buffer.head;
    rx_buffer.full = 0;

    return count;
}


static void handle_ctd_line(writefn_t writefn) {
    // Copy the line into contiguous memory
    char line[sizeof(LONGEST_CTD_STR)+1];
    size_t len = rb_read_all(line);
    line[len] = '\0';

    // Parse temperature, conductivity, and pressure
    char *buf_ptr = line;
    buf_ptr += strspn(buf_ptr, " ");
    samples[n_samples].temperature = atof(strsep(&buf_ptr, ","));
    buf_ptr += strspn(buf_ptr, " ");
    samples[n_samples].conductivity = atof(strsep(&buf_ptr, ","));
    buf_ptr += strspn(buf_ptr, " ");
    samples[n_samples].pressure = atof(strsep(&buf_ptr, ","));

    // If there is a fourth field, count the digits after the decimal point to
    // determine if it's salinity (sss.ssss) or sound velocity (vvvv.vvv).
    samples[n_samples].salinity = -9999;
    samples[n_samples].sound_velocity = -9999;

    buf_ptr += strspn(buf_ptr, " ");
    char *token = strsep(&buf_ptr, ",");
    if (token) {
        char *decimal = strchr(token, '.');
        if (decimal && strspn(decimal + 1, "0123456789") == 4)
            samples[n_samples].salinity = atof(token);
        else
            samples[n_samples].sound_velocity = atof(token);
    }

    // If there is a fifth field, it must be sound velocity
    buf_ptr += strspn(buf_ptr, " ");
    token = strsep(&buf_ptr, ",");
    if (token)
        samples[n_samples].sound_velocity = atof(token);

    n_samples ++;

    // If we filled our parsed samples buffer, emit the average
    if (n_samples == MAX_SAMPLES) {
        // Sum all samples into the first slot
        for (int i = 1; i < MAX_SAMPLES; i ++) {
            samples[0].temperature += samples[i].temperature;
            samples[0].conductivity += samples[i].conductivity;
            samples[0].pressure += samples[i].pressure;
            samples[0].salinity += samples[i].salinity;
            samples[0].sound_velocity += samples[i].sound_velocity;
        }

        // Compute the average of the samples
        samples[0].temperature /= MAX_SAMPLES;
        samples[0].conductivity /= MAX_SAMPLES;
        samples[0].pressure /= MAX_SAMPLES;
        samples[0].salinity /= MAX_SAMPLES;
        samples[0].sound_velocity /= MAX_SAMPLES;

        // Output the average
        buf_ptr = line;
        dtostrf(samples[0].temperature, 8, 4, buf_ptr);
        buf_ptr += 8;
        *buf_ptr++ = ',';
        *buf_ptr++ = ' ';
        dtostrf(samples[0].conductivity, 8, 5, buf_ptr);
        buf_ptr += 8;
        *buf_ptr++ = ',';
        *buf_ptr++ = ' ';
        dtostrf(samples[0].pressure, 8, 3, buf_ptr);
        buf_ptr += 8;
        *buf_ptr++ = ',';
        *buf_ptr++ = ' ';
        dtostrf(samples[0].salinity, 8, 4, buf_ptr);
        buf_ptr += 8;

        // Technically the Lander Control Board V1 firmware does not parse the
        // fifth value, but there shouldn't be any harm in emitting it.
        *buf_ptr++ = ',';
        *buf_ptr++ = ' ';
        dtostrf(samples[0].sound_velocity, 8, 3, buf_ptr);
        buf_ptr += 8;

        *buf_ptr++ = '\n';
        *buf_ptr++ = '\0';

        writefn(line);

        // Reset the insertion cursor
        n_samples = 0;
    }
}


void handle_ctd_input(writefn_t writefn, char *input, size_t len) {
    // Initialize the ring buffer head and tail pointers the first time
    if (!rx_buffer.head) {
        rx_buffer.head = rx_buffer.buffer;
        rx_buffer.tail = rx_buffer.buffer;
    }

    // Copy data into the receive buffer. When we hit a newline, process the
    // contents of the buffer.
    for (size_t i = 0; i < len; i ++) {
        if (input[i] == '\n')
            handle_ctd_line(writefn);
        else
            rxb_write(input[i]);
    }
}
