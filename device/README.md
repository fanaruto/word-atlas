# Word Atlas on ESP-Mosaico

Flash-card app for the ESP-Mosaico (ESP32-S31) with the two-button Interaction module.

| Where | What |
| --- | --- |
| `components/vocab_core/` | Hardware-free logic: button gestures, the card session and the light-sensor policy |
| `main/` | ESP-IDF app: word-pack loading, main loop, and `board_mosaico.c` (BSP adapter, **not wired yet**) |
| `host/` | PC build of `vocab_core`: unit tests and a terminal simulator |
| `pack/` | Generated word pack (git-ignored), flashed to the `storage` partition |

## Controls

| Card shows | Left | Right |
| --- | --- | --- |
| Word only | Don't know: show meaning, rate Again | I know it: show meaning to check |
| Meaning (checking) | Actually wrong: rate Again | Correct: rated Easy / Good / Hard by recall time |
| Meaning (after "don't know") | Next | Next |
| Long press | Undo last card | Play pronunciation |

Hold both for 1 s to pause or resume. Covering the light sensor also pauses. Recall time
under 2.5 s is Easy and over 9 s is Hard, matching `gradeFor()` in `src/store/session.ts`.
Cards rated Again come back three cards later, at most twice; only the first answer is recorded.

## Try it on a PC

```bash
python3 scripts/device/build_pack.py --limit 50 --out device/pack
cmake -S device/host -B device/host/build && cmake --build device/host/build
ctest --test-dir device/host/build
./device/host/build/sim device/pack/words.tsv   # a = left, l = right, A/L = long, b = both
```

## Build the firmware

```bash
python3 scripts/device/build_pack.py --limit 50 --out device/pack
cd device && idf.py set-target esp32s31 && idf.py build
```

Flash size (16 MB) and partition sizes in `partitions.csv` still need checking against the board.
