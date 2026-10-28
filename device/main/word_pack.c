#include "word_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "word_pack";

esp_err_t word_pack_load(const char *path, uint16_t max_words, word_pack_t *out) {
  memset(out, 0, sizeof(*out));
  FILE *f = fopen(path, "rb");
  if (!f) {
    ESP_LOGE(TAG, "can't open %s", path);
    return ESP_ERR_NOT_FOUND;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  out->buf = malloc(size + 1);
  out->words = calloc(max_words, sizeof(word_entry_t));
  if (!out->buf || !out->words) {
    fclose(f);
    word_pack_free(out);
    return ESP_ERR_NO_MEM;
  }
  size_t got = fread(out->buf, 1, size, f);
  fclose(f);
  out->buf[got] = 0;

  // Split in place: one line per word, eight tab-separated columns.
  char *line = out->buf;
  while (line && *line && out->count < max_words) {
    char *next = strchr(line, '\n');
    if (next) *next++ = 0;
    char *cols[8] = {0};
    char *p = line;
    for (int i = 0; i < 8 && p; i++) {
      cols[i] = p;
      p = strchr(p, '\t');
      if (p) *p++ = 0;
    }
    if (cols[1] && *cols[1]) {
      for (int i = 0; i < 8; i++) if (!cols[i]) cols[i] = "";
      out->words[out->count++] = (word_entry_t){
          cols[0], cols[1], cols[2], cols[3], cols[4], cols[5], cols[6], cols[7],
      };
    }
    line = next;
  }
  ESP_LOGI(TAG, "loaded %u words from %s", out->count, path);
  return out->count ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

void word_pack_free(word_pack_t *pack) {
  free(pack->words);
  free(pack->buf);
  memset(pack, 0, sizeof(*pack));
}
