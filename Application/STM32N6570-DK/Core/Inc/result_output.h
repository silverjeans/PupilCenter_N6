/**
 * @file    result_output.h
 * @brief   Formats and transmits a per-frame pupil detection record.
 *
 * Canonical one-line log format (ASCII, no trailing spaces):
 *
 *     F=<seq>,T=<ts_ms>,CX=<cx.2>,CY=<cy.2>,CONF=<0..1.2>,VALID=<0|1>,
 *     STATE=<tracking_state>,LAT=<last_latency_us>\r\n
 *
 * Example:
 *     F=12345,T=98765,CX=412.00,CY=238.00,CONF=0.87,VALID=1,STATE=1,LAT=312
 *
 * The formatter never allocates; callers provide the buffer. A convenience
 * emit function pushes the line through the UART backend.
 *
 * The backend is a single function pointer so this module stays PC-testable.
 */
#ifndef RESULT_OUTPUT_H
#define RESULT_OUTPUT_H

#include <stdint.h>
#include <stddef.h>
#include "app_types.h"

/** One line never exceeds this many bytes including trailing \r\n and NUL. */
#define RESULT_OUTPUT_LINE_MAX   128

/**
 * Byte-oriented write callback for the transport layer (UART, USB CDC, ...).
 * Return number of bytes written on success, negative on error.
 */
typedef int (*result_output_write_fn)(const uint8_t* data, size_t len);

/**
 * @brief Install the transport and prepare internal state.
 * @param write_fn  transport callback; may be NULL to format-only mode.
 * @return 0 on success, negative on error.
 */
int result_output_init(result_output_write_fn write_fn);

/**
 * @brief Format one pupil record into the canonical one-line ASCII form.
 *
 * Always NUL-terminates. Never writes past @p cap.
 *
 * @param seq        frame sequence number            (F=)
 * @param ts_ms      capture timestamp in milliseconds (T=)
 * @param r          detection record (required)
 * @param state      tracking_sm state as uint8 (SEARCH=0, TRACK=1, RECOVERY=2)
 * @param latency_us last measured end-to-end latency in microseconds
 * @param buf        destination buffer
 * @param cap        buffer capacity in bytes (must be >= 32 to be useful)
 * @return bytes written (excluding NUL).
 */
int result_output_format(uint32_t seq,
                         uint32_t ts_ms,
                         const pupil_result_t* r,
                         uint8_t state,
                         uint32_t latency_us,
                         char* buf,
                         size_t cap);

/**
 * @brief Format and transmit one pupil record via the installed transport.
 *        No-op if no transport was installed.
 * @return bytes transmitted, or negative on transport error.
 */
int result_output_emit(uint32_t seq,
                       uint32_t ts_ms,
                       const pupil_result_t* r,
                       uint8_t state,
                       uint32_t latency_us);

/**
 * @brief Transmit a free-form one-shot event line (e.g. state transitions).
 *        The library appends "\r\n"; caller supplies no newline.
 */
int result_output_emit_event(const char* fmt, ...);

#endif /* RESULT_OUTPUT_H */
