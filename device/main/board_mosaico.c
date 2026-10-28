// ESP-Mosaico adapter. NOT WIRED YET: every function below logs and returns a
// neutral value. Wire them to the BSP (esp-mosaico-bsp):
//   - inputs:     mosaico_module_interact, mosaico_interact_read_inputs()
//                 -> left_pressed, right_pressed, light_level (0-100)
//   - LEDs:       the module's WS2812s can mirror haptic feedback (green/red)
//   - display, haptics, audio: the host BSP
// until then the firmware builds and the pack loads, but nothing shows on screen.
#include "board.h"

#include "esp_log.h"

static const char *TAG = "board";

esp_err_t board_init(void) {
  ESP_LOGW(TAG, "board adapter not wired to the Mosaico BSP yet");
  return ESP_OK;
}

esp_err_t board_read_inputs(board_inputs_t *out) {
  *out = (board_inputs_t){.left = false, .right = false, .light_level = 50};
  return ESP_OK;
}

void board_draw_card(const card_view_t *view) {
  if (!view->word) {
    ESP_LOGI(TAG, "session done: %u/%u", view->done, view->total);
    return;
  }
  ESP_LOGI(TAG, "[%u/%u] %s %s%s", view->done, view->total, view->word->w, view->word->ipa,
           view->phase == PHASE_FRONT ? "" : view->word->zh);
}

void board_set_brightness(uint8_t pct) { (void)pct; }

void board_haptic(haptic_t pattern) { ESP_LOGD(TAG, "haptic %d", pattern); }

void board_play_audio(const char *path) { ESP_LOGI(TAG, "play %s", path); }

void board_show_wake(uint16_t due) { ESP_LOGI(TAG, "good morning: %u words today", due); }
