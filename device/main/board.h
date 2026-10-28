// Hardware the flash-card app needs from the ESP-Mosaico host and its
// Interaction module. board_mosaico.c maps these onto the BSP; everything
// above this line is hardware-agnostic.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "card_session.h"
#include "esp_err.h"
#include "word_pack.h"

typedef struct {
  bool left;
  bool right;
  uint8_t light_level;  // 0-100, relative
} board_inputs_t;

typedef struct {
  const word_entry_t *word;
  phase_t phase;
  uint16_t done;
  uint16_t total;
  bool paused;
  bool dark_theme;
} card_view_t;

esp_err_t board_init(void);
esp_err_t board_read_inputs(board_inputs_t *out);
void board_draw_card(const card_view_t *view);
void board_set_brightness(uint8_t pct);
void board_haptic(haptic_t pattern);
void board_play_audio(const char *path);
// Wake the screen and show how many words are waiting today.
void board_show_wake(uint16_t due);
