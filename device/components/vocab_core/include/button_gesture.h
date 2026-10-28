// Turns raw press/release edges from the two-button module into gestures.
//
//   short press (released before LONG_PRESS_MS)  -> GESTURE_LEFT / GESTURE_RIGHT
//   held for LONG_PRESS_MS                        -> GESTURE_*_LONG, fired while still held
//   both held together for CHORD_MS               -> GESTURE_BOTH_LONG
//
// A press that becomes part of a chord never fires its own short or long
// gesture. Pure C with no ESP-IDF dependency so it can be unit-tested on a PC.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GESTURE_DEBOUNCE_MS 30
#define GESTURE_LONG_PRESS_MS 600
#define GESTURE_CHORD_MS 1000

typedef enum { BTN_LEFT = 0, BTN_RIGHT = 1 } btn_id_t;

typedef enum {
  GESTURE_NONE = 0,
  GESTURE_LEFT,
  GESTURE_RIGHT,
  GESTURE_LEFT_LONG,
  GESTURE_RIGHT_LONG,
  GESTURE_BOTH_LONG,
} gesture_t;

typedef struct {
  bool down[2];
  int64_t down_at[2];
  int64_t edge_at[2];
  bool long_fired[2];
  bool chord;        // both buttons went down before either fired a long press
  int64_t chord_at;  // when the second button joined the chord
  bool chord_fired;
} gesture_decoder_t;

void gesture_init(gesture_decoder_t *d);

// Feed one debounced or raw edge. Returns a gesture completed by this edge.
gesture_t gesture_on_edge(gesture_decoder_t *d, btn_id_t btn, bool pressed, int64_t now_ms);

// Call every ~20 ms; long presses and chords fire from here while held.
gesture_t gesture_on_tick(gesture_decoder_t *d, int64_t now_ms);

const char *gesture_name(gesture_t g);

#ifdef __cplusplus
}
#endif
