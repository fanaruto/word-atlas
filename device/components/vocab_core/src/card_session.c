#include "card_session.h"

#include <string.h>

rating_t session_grade(uint32_t recall_ms) {
  if (recall_ms > SESSION_SLOW_MS) return RATING_HARD;
  if (recall_ms < SESSION_FAST_MS) return RATING_EASY;
  return RATING_GOOD;
}

static void show_front(card_session_t *s, int64_t now_ms) {
  session_state_t *c = &s->cur;
  if (c->pos >= c->queue_len) {
    c->phase = PHASE_DONE;
    return;
  }
  c->phase = PHASE_FRONT;
  c->shown_at = now_ms;
}

void session_start(card_session_t *s, uint16_t card_count, int64_t now_ms) {
  memset(s, 0, sizeof(*s));
  if (card_count > SESSION_MAX_CARDS) card_count = SESSION_MAX_CARDS;
  s->card_count = card_count;
  for (uint16_t i = 0; i < card_count; i++) s->cur.queue[i] = i;
  s->cur.queue_len = card_count;
  show_front(s, now_ms);
  s->front = s->cur;
}

// Insert a retry of `card` a few positions after the current one.
static void requeue(session_state_t *c, uint16_t card) {
  if (c->retries[card] >= SESSION_MAX_RETRIES || c->queue_len >= SESSION_MAX_QUEUE) return;
  c->retries[card]++;
  uint16_t at = c->pos + 1 + SESSION_RETRY_GAP;
  if (at > c->queue_len) at = c->queue_len;
  memmove(&c->queue[at + 1], &c->queue[at], (c->queue_len - at) * sizeof(c->queue[0]));
  c->queue[at] = card;
  c->queue_len++;
}

static void answer(card_session_t *s, rating_t rating, int64_t now_ms) {
  session_state_t *c = &s->cur;
  uint16_t card = c->queue[c->pos];
  if (!c->answered[card]) {
    c->answered[card] = true;
    c->log[c->log_len++] = (review_t){
        .card = card,
        .rating = (uint8_t)rating,
        .recall_ms = c->recall_ms,
        .at_ms = now_ms,
    };
  }
  if (rating == RATING_AGAIN) requeue(c, card);
}

static void next_card(card_session_t *s, int64_t now_ms) {
  s->undo = s->front;
  s->can_undo = true;
  s->cur.pos++;
  show_front(s, now_ms);
  s->front = s->cur;
}

session_effects_t session_on_gesture(card_session_t *s, gesture_t g, int64_t now_ms) {
  session_effects_t fx = {0};
  session_state_t *c = &s->cur;

  if (g == GESTURE_BOTH_LONG) return session_set_paused(s, !s->paused, now_ms);
  if (s->paused || g == GESTURE_NONE) return fx;

  if (g == GESTURE_LEFT_LONG) {
    if (!s->can_undo) return fx;
    s->cur = s->undo;
    s->cur.shown_at = now_ms;
    s->front = s->cur;
    s->can_undo = false;
    fx.redraw = true;
    fx.haptic = HAPTIC_UNDO;
    return fx;
  }
  if (c->phase == PHASE_DONE) return fx;
  if (g == GESTURE_RIGHT_LONG) {
    fx.play_audio = true;
    return fx;
  }

  fx.redraw = true;
  switch (c->phase) {
    case PHASE_FRONT:
      if (g == GESTURE_LEFT) {
        c->recall_ms = (uint32_t)(now_ms - c->shown_at);
        answer(s, RATING_AGAIN, now_ms);
        c->phase = PHASE_ANSWER;
        fx.haptic = HAPTIC_FAIL;
      } else {
        // Freeze the recall time now; checking the meaning doesn't count.
        c->recall_ms = (uint32_t)(now_ms - c->shown_at);
        c->phase = PHASE_CHECK;
        fx.haptic = HAPTIC_TICK;
      }
      break;
    case PHASE_CHECK:
      if (g == GESTURE_RIGHT) {
        answer(s, session_grade(c->recall_ms), now_ms);
        fx.haptic = HAPTIC_SUCCESS;
      } else {
        answer(s, RATING_AGAIN, now_ms);
        fx.haptic = HAPTIC_FAIL;
      }
      next_card(s, now_ms);
      break;
    case PHASE_ANSWER:
      fx.haptic = HAPTIC_TICK;
      next_card(s, now_ms);
      break;
    default:
      break;
  }
  return fx;
}

session_effects_t session_set_paused(card_session_t *s, bool paused, int64_t now_ms) {
  session_effects_t fx = {0};
  if (paused == s->paused) return fx;
  s->paused = paused;
  if (paused) {
    s->paused_at = now_ms;
  } else {
    // Time spent paused doesn't count toward recall time.
    int64_t away = now_ms - s->paused_at;
    s->cur.shown_at += away;
    s->front.shown_at += away;
  }
  fx.redraw = true;
  fx.haptic = HAPTIC_TICK;
  return fx;
}

int session_current_card(const card_session_t *s) {
  if (s->cur.phase == PHASE_DONE) return -1;
  return s->cur.queue[s->cur.pos];
}

phase_t session_phase(const card_session_t *s) { return s->cur.phase; }

uint16_t session_progress(const card_session_t *s) { return s->cur.log_len; }

const review_t *session_reviews(const card_session_t *s, uint16_t *count) {
  *count = s->cur.log_len;
  return s->cur.log;
}
