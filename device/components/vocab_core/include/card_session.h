// One study session on the device: a queue of cards driven by two-button
// gestures, producing FSRS-style ratings for the sync backend.
//
//   FRONT   word only.   left  -> "don't know": reveal, rate Again   -> ANSWER
//                        right -> "I know it": reveal for checking    -> CHECK
//   CHECK   meaning.     right -> correct: rate by recall time, next card
//                        left  -> actually wrong: rate Again, next card
//   ANSWER  meaning.     either -> next card
//
// Long right plays audio, long left undoes the last graded card, holding both
// pauses or resumes. A card rated Again comes back a few cards later (at most
// twice); like the web app, only the first answer of a card is recorded.
// Pure C with no ESP-IDF dependency.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "button_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SESSION_MAX_CARDS 128
#define SESSION_MAX_QUEUE (SESSION_MAX_CARDS * 3)
#define SESSION_RETRY_GAP 3
#define SESSION_MAX_RETRIES 2

// Recall-time thresholds, matching gradeFor() in src/store/session.ts.
#define SESSION_FAST_MS 2500
#define SESSION_SLOW_MS 9000

typedef enum { RATING_AGAIN = 1, RATING_HARD = 2, RATING_GOOD = 3, RATING_EASY = 4 } rating_t;

typedef enum { PHASE_FRONT, PHASE_CHECK, PHASE_ANSWER, PHASE_DONE } phase_t;

typedef enum { HAPTIC_NONE, HAPTIC_TICK, HAPTIC_SUCCESS, HAPTIC_FAIL, HAPTIC_UNDO } haptic_t;

typedef struct {
  uint16_t card;      // index into the caller's word list
  uint8_t rating;     // rating_t
  uint32_t recall_ms; // word shown -> first answer, pauses excluded
  int64_t at_ms;      // caller's clock when answered
} review_t;

typedef struct {
  uint16_t queue[SESSION_MAX_QUEUE];
  uint16_t queue_len;
  uint16_t pos;
  uint8_t retries[SESSION_MAX_CARDS];
  bool answered[SESSION_MAX_CARDS];
  review_t log[SESSION_MAX_CARDS];
  uint16_t log_len;
  phase_t phase;
  int64_t shown_at;    // when the current front appeared, shifted by pauses
  uint32_t recall_ms;  // frozen when the current card is first answered
} session_state_t;

typedef struct {
  session_state_t cur;
  session_state_t undo;  // state at the previous card's front
  session_state_t front; // state at the current card's front
  bool can_undo;
  bool paused;
  int64_t paused_at;
  uint16_t card_count;
} card_session_t;

// What the UI layer should do after a gesture.
typedef struct {
  bool redraw;
  bool play_audio;
  haptic_t haptic;
} session_effects_t;

void session_start(card_session_t *s, uint16_t card_count, int64_t now_ms);
session_effects_t session_on_gesture(card_session_t *s, gesture_t g, int64_t now_ms);
session_effects_t session_set_paused(card_session_t *s, bool paused, int64_t now_ms);

// Current card index, or -1 when the session is done.
int session_current_card(const card_session_t *s);
phase_t session_phase(const card_session_t *s);
// Cards answered for the first time / total cards.
uint16_t session_progress(const card_session_t *s);

const review_t *session_reviews(const card_session_t *s, uint16_t *count);

rating_t session_grade(uint32_t recall_ms);

#ifdef __cplusplus
}
#endif
