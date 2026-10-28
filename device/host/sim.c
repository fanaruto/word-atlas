// Terminal stand-in for the device, to try the two-button flow on a PC.
//
//   ./sim device/pack/words.tsv
//   a = left   l = right   A = long left (undo)   L = long right (audio)   b = both (pause)
//
// Real time is used for recall timing, so answering fast or slow changes the rating.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "card_session.h"

#define MAX_LINE 1024

typedef struct {
  char w[64], ipa[64], zh[256], ex[512];
} word_t;

static word_t words[SESSION_MAX_CARDS];

static int64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void copy_field(char *dst, size_t n, const char *src) {
  snprintf(dst, n, "%s", src ? src : "");
}

static int load(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) return -1;
  char line[MAX_LINE];
  int n = 0;
  // Columns: id, w, ipa, zh, ex, exZh, img, audio (see scripts/device/build_pack.py).
  while (n < SESSION_MAX_CARDS && fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    char *cols[8] = {0};
    char *p = line;
    for (int i = 0; i < 8 && p; i++) {
      cols[i] = p;
      p = strchr(p, '\t');
      if (p) *p++ = 0;
    }
    copy_field(words[n].w, sizeof words[n].w, cols[1]);
    copy_field(words[n].ipa, sizeof words[n].ipa, cols[2]);
    copy_field(words[n].zh, sizeof words[n].zh, cols[3]);
    copy_field(words[n].ex, sizeof words[n].ex, cols[4]);
    n++;
  }
  fclose(f);
  return n;
}

static void draw(const card_session_t *s, int total) {
  int card = session_current_card(s);
  printf("\n[%u/%d]%s ", session_progress(s), total, s->paused ? " (paused)" : "");
  if (card < 0) {
    puts("done");
    return;
  }
  const word_t *w = &words[card];
  printf("%s  %s\n", w->w, w->ipa);
  if (session_phase(s) != PHASE_FRONT) printf("  %s\n  %s\n", w->zh, w->ex);
  printf("%s> ", session_phase(s) == PHASE_FRONT ? "a=don't know  l=know" : session_phase(s) == PHASE_CHECK ? "l=right  a=wrong" : "any key=next");
  fflush(stdout);
}

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "device/pack/words.tsv";
  int total = load(path);
  if (total <= 0) {
    fprintf(stderr, "can't read %s; run scripts/device/build_pack.py first\n", path);
    return 1;
  }
  static card_session_t s;
  session_start(&s, (uint16_t)total, now_ms());
  draw(&s, total);

  int c;
  while ((c = getchar()) != EOF && session_phase(&s) != PHASE_DONE) {
    gesture_t g = c == 'a' ? GESTURE_LEFT
                : c == 'l' ? GESTURE_RIGHT
                : c == 'A' ? GESTURE_LEFT_LONG
                : c == 'L' ? GESTURE_RIGHT_LONG
                : c == 'b' ? GESTURE_BOTH_LONG
                : GESTURE_NONE;
    if (g == GESTURE_NONE) continue;
    session_effects_t fx = session_on_gesture(&s, g, now_ms());
    if (fx.play_audio) printf("(plays %s)\n", words[session_current_card(&s)].w);
    if (fx.redraw) draw(&s, total);
  }

  uint16_t n;
  const review_t *log = session_reviews(&s, &n);
  static const char *names[] = {"", "again", "hard", "good", "easy"};
  puts("\nreviews:");
  for (uint16_t i = 0; i < n; i++)
    printf("  %-16s %-5s %5u ms\n", words[log[i].card].w, names[log[i].rating], (unsigned)log[i].recall_ms);
  return 0;
}
