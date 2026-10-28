#include <stdio.h>
#include <stdlib.h>

#include "button_gesture.h"
#include "card_session.h"
#include "light_policy.h"

static int failures;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
      failures++;                                                \
    }                                                            \
  } while (0)

// Press `btn` at t, release at t + hold, ticking every 20 ms; returns the last gesture seen.
static gesture_t press(gesture_decoder_t *d, btn_id_t btn, int64_t t, int64_t hold) {
  gesture_t seen = gesture_on_edge(d, btn, true, t);
  for (int64_t x = t + 20; x < t + hold; x += 20) {
    gesture_t g = gesture_on_tick(d, x);
    if (g != GESTURE_NONE) seen = g;
  }
  gesture_t g = gesture_on_edge(d, btn, false, t + hold);
  return g != GESTURE_NONE ? g : seen;
}

static void test_gestures(void) {
  gesture_decoder_t d;
  gesture_init(&d);
  CHECK(press(&d, BTN_LEFT, 1000, 120) == GESTURE_LEFT);
  CHECK(press(&d, BTN_RIGHT, 2000, 120) == GESTURE_RIGHT);
  CHECK(press(&d, BTN_RIGHT, 3000, 900) == GESTURE_RIGHT_LONG);
  CHECK(press(&d, BTN_LEFT, 4000, 900) == GESTURE_LEFT_LONG);

  // Bounce within the debounce window is ignored.
  gesture_init(&d);
  CHECK(gesture_on_edge(&d, BTN_LEFT, true, 1000) == GESTURE_NONE);
  CHECK(gesture_on_edge(&d, BTN_LEFT, false, 1010) == GESTURE_NONE);
  CHECK(gesture_on_edge(&d, BTN_LEFT, false, 1100) == GESTURE_LEFT);

  // Both held: one chord gesture, no short or long gestures from either button.
  gesture_init(&d);
  int both = 0, other = 0;
  gesture_t g = gesture_on_edge(&d, BTN_LEFT, true, 1000);
  g = gesture_on_edge(&d, BTN_RIGHT, true, 1100);
  for (int64_t t = 1120; t < 2500; t += 20) {
    g = gesture_on_tick(&d, t);
    if (g == GESTURE_BOTH_LONG) both++;
    else if (g != GESTURE_NONE) other++;
  }
  g = gesture_on_edge(&d, BTN_LEFT, false, 2500);
  if (g != GESTURE_NONE) other++;
  g = gesture_on_edge(&d, BTN_RIGHT, false, 2550);
  if (g != GESTURE_NONE) other++;
  CHECK(both == 1);
  CHECK(other == 0);
  // The decoder is usable again after the chord.
  CHECK(press(&d, BTN_RIGHT, 3000, 100) == GESTURE_RIGHT);
}

static void test_session_flow(void) {
  static card_session_t s;
  session_start(&s, 3, 0);
  CHECK(session_current_card(&s) == 0);
  CHECK(session_phase(&s) == PHASE_FRONT);

  // Card 0: known quickly and confirmed -> Easy.
  session_on_gesture(&s, GESTURE_RIGHT, 1000);
  CHECK(session_phase(&s) == PHASE_CHECK);
  session_effects_t fx = session_on_gesture(&s, GESTURE_RIGHT, 6000);
  CHECK(fx.haptic == HAPTIC_SUCCESS);

  // Card 1: slow recall -> Hard; checking time is not counted.
  CHECK(session_current_card(&s) == 1);
  session_on_gesture(&s, GESTURE_RIGHT, 6000 + 10000);
  session_on_gesture(&s, GESTURE_RIGHT, 6000 + 10000 + 30000);

  // Card 2: "don't know" -> Again, answer shown, then comes back once.
  CHECK(session_current_card(&s) == 2);
  fx = session_on_gesture(&s, GESTURE_LEFT, 50000);
  CHECK(fx.haptic == HAPTIC_FAIL);
  CHECK(session_phase(&s) == PHASE_ANSWER);
  session_on_gesture(&s, GESTURE_RIGHT, 52000);
  CHECK(session_current_card(&s) == 2);  // retry
  session_on_gesture(&s, GESTURE_RIGHT, 53000);
  session_on_gesture(&s, GESTURE_RIGHT, 54000);
  CHECK(session_phase(&s) == PHASE_DONE);
  CHECK(session_current_card(&s) == -1);

  uint16_t n;
  const review_t *log = session_reviews(&s, &n);
  CHECK(n == 3);  // the retry is not recorded
  CHECK(log[0].card == 0 && log[0].rating == RATING_EASY && log[0].recall_ms == 1000);
  CHECK(log[1].card == 1 && log[1].rating == RATING_HARD && log[1].recall_ms == 10000);
  CHECK(log[2].card == 2 && log[2].rating == RATING_AGAIN);
}

static void test_session_wrong_after_check(void) {
  static card_session_t s;
  session_start(&s, 2, 0);
  session_on_gesture(&s, GESTURE_RIGHT, 4000);
  session_on_gesture(&s, GESTURE_LEFT, 5000);  // "actually wrong"
  uint16_t n;
  const review_t *log = session_reviews(&s, &n);
  CHECK(n == 1 && log[0].rating == RATING_AGAIN);
  CHECK(session_current_card(&s) == 1);
}

static void test_undo_audio_pause(void) {
  static card_session_t s;
  session_start(&s, 3, 0);
  CHECK(session_on_gesture(&s, GESTURE_LEFT_LONG, 100).haptic == HAPTIC_NONE);  // nothing to undo yet
  CHECK(session_on_gesture(&s, GESTURE_RIGHT_LONG, 200).play_audio);

  session_on_gesture(&s, GESTURE_RIGHT, 1000);
  session_on_gesture(&s, GESTURE_LEFT, 2000);  // slipped: meant right
  CHECK(session_current_card(&s) == 1);
  session_effects_t fx = session_on_gesture(&s, GESTURE_LEFT_LONG, 3000);
  CHECK(fx.haptic == HAPTIC_UNDO);
  CHECK(session_current_card(&s) == 0);
  CHECK(session_phase(&s) == PHASE_FRONT);
  uint16_t n;
  session_reviews(&s, &n);
  CHECK(n == 0);

  // Paused time is excluded from recall time; gestures are ignored while paused.
  session_on_gesture(&s, GESTURE_BOTH_LONG, 4000);  // card front shown again at 3000
  CHECK(session_on_gesture(&s, GESTURE_RIGHT, 5000).redraw == false);
  session_on_gesture(&s, GESTURE_BOTH_LONG, 60000);
  session_on_gesture(&s, GESTURE_RIGHT, 61000);
  session_on_gesture(&s, GESTURE_RIGHT, 62000);
  const review_t *log = session_reviews(&s, &n);
  CHECK(n == 1 && log[0].recall_ms == 2000 && log[0].rating == RATING_EASY);
}

static void test_light(void) {
  light_policy_t p;
  light_init(&p);
  light_out_t o = light_update(&p, 80, 0, 14, 1);
  CHECK(!o.dark_theme && !o.covered && o.brightness_pct > 80);

  // Covered for long enough -> covered once; uncovered -> back.
  int changes = 0;
  for (int64_t t = 100; t < 2000; t += 100) {
    o = light_update(&p, 0, t, 14, 1);
    if (o.covered_changed) changes++;
  }
  CHECK(o.covered && changes == 1);
  o = light_update(&p, 80, 2100, 14, 1);
  CHECK(!o.covered && o.covered_changed);

  // Dim room -> dark theme after smoothing settles.
  for (int64_t t = 3000; t < 6000; t += 100) o = light_update(&p, 10, t, 21, 1);
  CHECK(o.dark_theme && o.mute_auto_audio);

  // Night, then lights on in the morning -> one wake per day.
  light_init(&p);
  int64_t t = 0;
  for (; t < 40LL * 60 * 1000; t += 60 * 1000) light_update(&p, 1, t, 3, 1);
  int wakes = 0;
  for (int i = 0; i < 30; i++, t += 1000) {
    o = light_update(&p, 90, t, 7, 2);
    if (o.morning_wake) wakes++;
  }
  CHECK(wakes == 1);

  // A dark afternoon nap does not count as morning.
  light_init(&p);
  for (t = 0; t < 40LL * 60 * 1000; t += 60 * 1000) light_update(&p, 1, t, 14, 3);
  wakes = 0;
  for (int i = 0; i < 30; i++, t += 1000) wakes += light_update(&p, 90, t, 15, 3).morning_wake;
  CHECK(wakes == 0);
}

int main(void) {
  test_gestures();
  test_session_flow();
  test_session_wrong_after_check();
  test_undo_audio_pause();
  test_light();
  if (failures) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  puts("vocab_core: all checks passed");
  return EXIT_SUCCESS;
}
