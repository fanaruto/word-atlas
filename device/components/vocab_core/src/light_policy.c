#include "light_policy.h"

void light_init(light_policy_t *p) {
  *p = (light_policy_t){.below_cover_since = -1, .dark_since = -1, .last_wake_day = -1};
}

light_out_t light_update(light_policy_t *p, uint8_t level, int64_t now_ms, int local_hour, int local_day) {
  light_out_t out = {0};
  if (level > 100) level = 100;

  // Covering the sensor is a deliberate gesture, so it uses the raw reading.
  if (level < LIGHT_COVERED_BELOW) {
    if (p->below_cover_since < 0) p->below_cover_since = now_ms;
    if (!p->covered && now_ms - p->below_cover_since >= LIGHT_COVERED_HOLD_MS) {
      p->covered = true;
      out.covered_changed = true;
    }
  } else {
    p->below_cover_since = -1;
    if (p->covered && level > LIGHT_UNCOVERED_ABOVE) {
      p->covered = false;
      out.covered_changed = true;
    }
  }

  // Everything else follows a smoothed reading so a passing shadow doesn't flicker the screen.
  if (!p->primed) {
    p->smoothed = level;
    p->primed = true;
  } else if (!p->covered) {
    p->smoothed += (level - p->smoothed) * 0.2f;
  }
  float s = p->smoothed;

  if (p->dim && s > LIGHT_BRIGHT_ABOVE) p->dim = false;
  else if (!p->dim && s < LIGHT_DIM_BELOW) p->dim = true;

  if (s < LIGHT_NIGHT_BELOW) {
    if (p->dark_since < 0) p->dark_since = now_ms;
  } else {
    bool long_night = p->dark_since >= 0 && now_ms - p->dark_since >= LIGHT_NIGHT_HOLD_MS;
    bool morning = local_hour >= LIGHT_MORNING_FROM_HOUR && local_hour < LIGHT_MORNING_TO_HOUR;
    if (long_night && morning && s > LIGHT_MORNING_ABOVE && p->last_wake_day != local_day) {
      p->last_wake_day = local_day;
      out.morning_wake = true;
    }
    if (s > LIGHT_MORNING_ABOVE || !long_night) p->dark_since = -1;
  }

  int pct = LIGHT_MIN_BRIGHTNESS + (int)(s * (100 - LIGHT_MIN_BRIGHTNESS) / 100.0f);
  out.brightness_pct = (uint8_t)(pct > 100 ? 100 : pct);
  out.dark_theme = p->dim;
  out.mute_auto_audio = p->dim;
  out.covered = p->covered;
  return out;
}
