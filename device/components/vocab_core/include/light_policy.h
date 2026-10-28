// Maps the Interaction module's relative light level (0-100, not lux) to
// screen behaviour:
//   - brightness follows the room, dark theme and no auto-audio when dim
//   - near-zero light held for a moment means the screen is covered: pause
//   - the room going from dark to bright in the morning wakes the screen once a day
// Thresholds are first guesses; tune them on the real sensor.
// Pure C with no ESP-IDF dependency.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIGHT_COVERED_BELOW 3
#define LIGHT_COVERED_HOLD_MS 1500
#define LIGHT_UNCOVERED_ABOVE 8
#define LIGHT_DIM_BELOW 20
#define LIGHT_BRIGHT_ABOVE 30
#define LIGHT_NIGHT_BELOW 10
#define LIGHT_NIGHT_HOLD_MS (30 * 60 * 1000)
#define LIGHT_MORNING_ABOVE 40
#define LIGHT_MORNING_FROM_HOUR 5
#define LIGHT_MORNING_TO_HOUR 11
#define LIGHT_MIN_BRIGHTNESS 15

typedef struct {
  bool covered;
  bool dim;
  int64_t below_cover_since;  // -1 when not below the covered threshold
  int64_t dark_since;         // -1 when not dark
  int last_wake_day;          // caller's local day number of the last morning wake
  float smoothed;
  bool primed;
} light_policy_t;

typedef struct {
  uint8_t brightness_pct;
  bool dark_theme;
  bool mute_auto_audio;
  bool covered;
  bool covered_changed;
  bool morning_wake;
} light_out_t;

void light_init(light_policy_t *p);

// level: sensor reading 0-100. local_hour 0-23 and local_day (any day counter
// that changes at local midnight) come from the caller's clock.
light_out_t light_update(light_policy_t *p, uint8_t level, int64_t now_ms, int local_hour, int local_day);

#ifdef __cplusplus
}
#endif
