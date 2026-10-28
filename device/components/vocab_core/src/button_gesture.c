#include "button_gesture.h"

#include <string.h>

void gesture_init(gesture_decoder_t *d) {
  memset(d, 0, sizeof(*d));
  d->edge_at[0] = d->edge_at[1] = -GESTURE_DEBOUNCE_MS;
}

gesture_t gesture_on_edge(gesture_decoder_t *d, btn_id_t btn, bool pressed, int64_t now_ms) {
  if (pressed == d->down[btn]) return GESTURE_NONE;
  if (now_ms - d->edge_at[btn] < GESTURE_DEBOUNCE_MS) return GESTURE_NONE;
  d->edge_at[btn] = now_ms;
  d->down[btn] = pressed;

  btn_id_t other = btn == BTN_LEFT ? BTN_RIGHT : BTN_LEFT;
  if (pressed) {
    d->down_at[btn] = now_ms;
    d->long_fired[btn] = false;
    if (d->down[other] && !d->long_fired[other]) {
      d->chord = true;
      d->chord_at = now_ms;
      d->chord_fired = false;
    }
    return GESTURE_NONE;
  }

  if (d->chord) {
    // The chord ends once both buttons are up; neither release is a gesture.
    if (!d->down[other]) d->chord = false;
    return GESTURE_NONE;
  }
  if (d->long_fired[btn]) return GESTURE_NONE;
  return btn == BTN_LEFT ? GESTURE_LEFT : GESTURE_RIGHT;
}

gesture_t gesture_on_tick(gesture_decoder_t *d, int64_t now_ms) {
  if (d->chord) {
    if (!d->chord_fired && d->down[0] && d->down[1] && now_ms - d->chord_at >= GESTURE_CHORD_MS) {
      d->chord_fired = true;
      return GESTURE_BOTH_LONG;
    }
    return GESTURE_NONE;
  }
  for (int b = 0; b < 2; b++) {
    if (d->down[b] && !d->long_fired[b] && now_ms - d->down_at[b] >= GESTURE_LONG_PRESS_MS) {
      d->long_fired[b] = true;
      return b == BTN_LEFT ? GESTURE_LEFT_LONG : GESTURE_RIGHT_LONG;
    }
  }
  return GESTURE_NONE;
}

const char *gesture_name(gesture_t g) {
  switch (g) {
    case GESTURE_LEFT: return "left";
    case GESTURE_RIGHT: return "right";
    case GESTURE_LEFT_LONG: return "left-long";
    case GESTURE_RIGHT_LONG: return "right-long";
    case GESTURE_BOTH_LONG: return "both-long";
    default: return "none";
  }
}
