# Builds the device logic for the PC so it can be tested without hardware:
#   cmake -S device/host -B device/host/build && cmake --build device/host/build && ctest --test-dir device/host/build
cmake_minimum_required(VERSION 3.16)
project(vocab_core_host C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -Werror)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../components/vocab_core)
add_library(vocab_core
  ${CORE}/src/button_gesture.c
  ${CORE}/src/card_session.c
  ${CORE}/src/light_policy.c)
target_include_directories(vocab_core PUBLIC ${CORE}/include)

enable_testing()
add_executable(test_vocab_core test_vocab_core.c)
target_link_libraries(test_vocab_core vocab_core)
add_test(NAME vocab_core COMMAND test_vocab_core)

add_executable(sim sim.c)
target_link_libraries(sim vocab_core)
