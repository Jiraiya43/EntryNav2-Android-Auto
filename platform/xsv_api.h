// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// C interface of the car's video library /usr/lib/libxsvideo.so.4 (no header ships with it).

// Declared from the library's exported symbols and from the way /usr/bin/early-video.digital calls it.
//
// The library registers 4 drivers by type (0 analog camera, 1 digital camera / H.264 decoder, 2 file player,
// 3 still picture). xsv_open(type, surface_id, title) returns an opaque handle.
// The type 1 driver connects to Wayland itself (ivi_application_interface / ivi_surface_interface), decodes H.264 on
// the IVA-HD through DCE/VIDDEC3 and renders NV12 onto the requested IVI surface. It is fed the Annex B H.264 stream
// in a dma-buf (see omap_dmabuf.hpp); it splits the NAL units itself (start codes 00 00 01 / 00 00 00 01) and skips
// AUD/SEI.
//
// ARM hard-float ABI, checked against the car's library: xsv_configure_stream takes
// handle/width/height in r0/r1/r2 and fps as a FLOAT in s0.
#ifndef ENTRY_AAD_PLATFORM_XSV_API_H
#define ENTRY_AAD_PLATFORM_XSV_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Driver types (first argument of xsv_open).
enum {
    XSV_TYPE_ANALOG_CAM = 0,
    XSV_TYPE_DIGITAL_CAM = 1,  // H.264 decoder onto an IVI surface: the one used here
    XSV_TYPE_FILE_PLAYER = 2,
    XSV_TYPE_STATIC_PIC = 3
};

// Events returned by xsv_receive_event (first word of the event).
enum {
    XSV_EVENT_PLAYING = 1,  // type 1: STATE_PLAYING, emitted before any H.264; NOT the first picture
    XSV_EVENT_STOPPED = 2,
    XSV_EVENT_ERROR = 5,  // followed by an error code
    XSV_EVENT_WATCHDOG_ERROR = 6,
    XSV_EVENT_ABORTED = 7,
    XSV_EVENT_DMABUF_COMPLETE = 9,  // the dma-buf handed over was consumed and can be reused
    XSV_EVENT_FRAME = 10            // Wayland frame callback; only event[0] initialized, no payload
};

using xsv_handle = void*;

// Opens a stream of the given type on IVI surface surface_id; title names it in the library's log.
// NULL when the type is not supported or the driver fails to initialise.
xsv_handle xsv_open(int type, uint32_t surface_id, const char* title);
void xsv_close(xsv_handle h);

// Queues the stream configuration. early-video passes 0.0f as the frame rate.
int xsv_configure_stream(xsv_handle h, uint32_t width, uint32_t height, float fps);

// Size of the displayed output (opcode 8) and input viewport (opcode 7).
int xsv_set_output_dimension(xsv_handle h, uint32_t width, uint32_t height);
int xsv_set_input_viewport(xsv_handle h, uint32_t x, uint32_t y, uint32_t w, uint32_t hgt);

// Submits one encoded picture: fd of a dma-buf holding length bytes of Annex B H.264.
// XSV_EVENT_DMABUF_COMPLETE reports when the buffer is free again.
int xsv_decode_video_frame(xsv_handle h, int dmabuf_fd, uint32_t length);

int xsv_start(xsv_handle h);
int xsv_stop(xsv_handle h);
int xsv_pause(xsv_handle h);
int xsv_update(xsv_handle h);  // applies the pending settings (opcode 0x15)

int xsv_enable_frame_events(xsv_handle h);
int xsv_disable_frame_events(xsv_handle h);

// Event queue: either a bounded blocking wait, or the fd for a poll() loop.
int xsv_receive_event(xsv_handle h, int* event_out, int timeout_ms);
int xsv_get_event_queue_fd(xsv_handle h);

#ifdef __cplusplus
}
#endif

#endif  // ENTRY_AAD_PLATFORM_XSV_API_H
