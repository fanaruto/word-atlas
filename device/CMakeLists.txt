# ESP-IDF project for the ESP-Mosaico flash-card app.
#   python3 scripts/device/build_pack.py --out device/pack   # word pack -> storage partition
#   cd device && idf.py set-target esp32s31 && idf.py build
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(word_atlas_mosaico)
