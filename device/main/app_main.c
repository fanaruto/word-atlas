// Flash-card loop for ESP-Mosaico: poll the two buttons and the light sensor,
// turn edges into gestures, drive the session and redraw.
#include <time.h>

#include "board.h"
#include "button_gesture.h"
#include "card_session.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "light_policy.h"
#include "word_pack.h"

static const char *TAG = "app";

#define TICK_MS 20
#define LIGHT_EVERY_TICKS 10
#define PACK_ROOT "/pack"

static word_pack_t pack;
static card_session_t session;
static gesture_decoder_t gestures;
static light_policy_t light;
static bool dark_theme;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static void redraw(void) {
  int card = session_current_card(&session);
  card_view_t view = {
      .word = card >= 0 ? &pack.words[card] : NULL,
      .phase = session_phase(&session),
      .done = session_progress(&session),
      .total = pack.count,
      .paused = session.paused,
      .dark_theme = dark_theme,
  };
  board_draw_card(&view);
}

static void apply(session_effects_t fx) {
  if (fx.haptic != HAPTIC_NONE) board_haptic(fx.haptic);
  if (fx.play_audio) {
    int card = session_current_card(&session);
    if (card >= 0 && pack.words[card].audio[0]) {
      char path[96];
      snprintf(path, sizeof path, PACK_ROOT "/%s", pack.words[card].audio);
      board_play_audio(path);
    }
  }
  if (fx.redraw) redraw();
}

static void poll_light(uint8_t level, int64_t t) {
  // Until the clock is synced over Wi-Fi, time() is near 1970 and the hour is
  // meaningless; the morning wake then simply won't fire.
  time_t wall = time(NULL);
  struct tm local;
  localtime_r(&wall, &local);
  light_out_t o = light_update(&light, level, t, local.tm_hour, local.tm_yday);
  board_set_brightness(o.brightness_pct);
  if (o.covered_changed) apply(session_set_paused(&session, o.covered, t));
  if (o.dark_theme != dark_theme) {
    dark_theme = o.dark_theme;
    redraw();
  }
  if (o.morning_wake) board_show_wake(pack.count - session_progress(&session));
}

static esp_err_t mount_pack(void) {
  esp_vfs_spiffs_conf_t conf = {
      .base_path = PACK_ROOT,
      .partition_label = "storage",
      .max_files = 4,
      .format_if_mount_failed = false,
  };
  return esp_vfs_spiffs_register(&conf);
}

void app_main(void) {
  ESP_ERROR_CHECK(board_init());
  ESP_ERROR_CHECK(mount_pack());
  ESP_ERROR_CHECK(word_pack_load(PACK_ROOT "/words.tsv", SESSION_MAX_CARDS, &pack));

  gesture_init(&gestures);
  light_init(&light);
  session_start(&session, pack.count, now_ms());
  redraw();

  bool was[2] = {false, false};
  for (uint32_t tick = 0;; tick++) {
    int64_t t = now_ms();
    board_inputs_t in;
    bool read_ok = board_read_inputs(&in) == ESP_OK;
    if (read_ok) {
      bool is[2] = {in.left, in.right};
      for (int b = 0; b < 2; b++) {
        if (is[b] != was[b]) {
          apply(session_on_gesture(&session, gesture_on_edge(&gestures, b, is[b], t), t));
          was[b] = is[b];
        }
      }
    }
    gesture_t g = gesture_on_tick(&gestures, t);
    if (g != GESTURE_NONE) {
      ESP_LOGD(TAG, "gesture %s", gesture_name(g));
      apply(session_on_gesture(&session, g, t));
    }
    if (read_ok && tick % LIGHT_EVERY_TICKS == 0) poll_light(in.light_level, t);
    vTaskDelay(pdMS_TO_TICKS(TICK_MS));
  }
}
