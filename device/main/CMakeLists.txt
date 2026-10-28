idf_component_register(
  SRCS "app_main.c" "word_pack.c" "board_mosaico.c"
  INCLUDE_DIRS "."
  REQUIRES vocab_core esp_timer spiffs
)
# Flash the word pack built by scripts/device/build_pack.py into the "storage" partition.
spiffs_create_partition_image(storage ${CMAKE_CURRENT_SOURCE_DIR}/../pack FLASH_IN_PROJECT)
