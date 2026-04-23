/**
 * @file    result_output.c
 * @brief   Implementation of the pupil-record one-line logger.
 *
 * Design notes:
 *   - No heap. One static scratch buffer per call-context is enough because
 *     the pipeline is single-threaded.
 *   - printf family is invoked with explicit casts so the GCC hardening
 *     warnings about signed/unsigned do not fire on newlib-nano.
 *   - Floats appear only where the format mandates them (CX/CY/CONF). The
 *     rest of the pipeline stays integer.
 *   - When the transport is not installed the module becomes format-only,
 *     useful for PC-side unit tests.
 */
#include "result_output.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ state */

static result_output_write_fn s_write_fn = 0;
/* Scratch buffer used only by emit paths; formatted output is written here
 * before being handed to the transport. Sized to two log lines for event
 * printf overflow safety. */
static char s_scratch[2 * RESULT_OUTPUT_LINE_MAX];

/* ----------------------------------------------------------- public API */

int result_output_init(result_output_write_fn write_fn)
{
    s_write_fn = write_fn;
    return 0;
}

int result_output_format(uint32_t seq,
                         uint32_t ts_ms,
                         const pupil_result_t* r,
                         uint8_t state,
                         uint32_t latency_us,
                         char* buf,
                         size_t cap)
{
    if (!r || !buf || cap < 32) {
        if (buf && cap) buf[0] = '\0';
        return 0;
    }

    /* CX/CY are stored as int16 pixel positions today, so the fractional
     * part is always zero. The format still prints .00 so downstream
     * parsers handle a future sub-pixel promotion without code changes. */
    float cx_f   = (float)r->cx;
    float cy_f   = (float)r->cy;
    float conf_f = (float)r->confidence / 255.0f;

    int n = snprintf(buf, cap,
                     "F=%lu,T=%lu,CX=%.2f,CY=%.2f,CONF=%.2f,VALID=%u,STATE=%u,LAT=%lu\r\n",
                     (unsigned long)seq, (unsigned long)ts_ms,
                     (double)cx_f, (double)cy_f, (double)conf_f,
                     (unsigned)r->valid,
                     (unsigned)state,
                     (unsigned long)latency_us);

    if (n < 0) { buf[0] = '\0'; return 0; }
    if ((size_t)n >= cap) n = (int)cap - 1;
    return n;
}

int result_output_emit(uint32_t seq,
                       uint32_t ts_ms,
                       const pupil_result_t* r,
                       uint8_t state,
                       uint32_t latency_us)
{
    int n = result_output_format(seq, ts_ms, r, state, latency_us,
                                 s_scratch, sizeof(s_scratch));
    if (n <= 0) return 0;
    if (!s_write_fn) return 0;
    return s_write_fn((const uint8_t*)s_scratch, (size_t)n);
}

int result_output_emit_event(const char* fmt, ...)
{
    if (!fmt) return 0;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(s_scratch, sizeof(s_scratch) - 2, fmt, ap);
    va_end(ap);

    if (n < 0) return 0;
    if ((size_t)n >= sizeof(s_scratch) - 2) n = (int)sizeof(s_scratch) - 3;

    s_scratch[n++] = '\r';
    s_scratch[n++] = '\n';

    if (!s_write_fn) return 0;
    return s_write_fn((const uint8_t*)s_scratch, (size_t)n);
}
