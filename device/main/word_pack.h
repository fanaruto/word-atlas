// Reads words.tsv from the word pack (see scripts/device/build_pack.py).
#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
  char *id, *w, *ipa, *zh, *ex, *ex_zh, *img, *audio;
} word_entry_t;

typedef struct {
  word_entry_t *words;
  uint16_t count;
  char *buf;  // owns every string above
} word_pack_t;

esp_err_t word_pack_load(const char *path, uint16_t max_words, word_pack_t *out);
void word_pack_free(word_pack_t *pack);
