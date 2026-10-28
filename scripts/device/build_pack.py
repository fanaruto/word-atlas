#!/usr/bin/env python3
"""Build a compact word pack for the ESP-Mosaico device.

The device has a 480x480 screen and limited flash, so each word keeps only
what the flash-card loop needs: headword, US IPA, up to two Chinese senses and
one short example, as words.json and words.tsv. Illustrations are re-encoded as small baseline JPEGs (the
ESP32-S31 has a hardware JPEG decoder) and word audio is copied as-is.

    python3 scripts/device/build_pack.py --limit 50 --out device/pack
"""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CEFR = ["A1", "A2", "B1", "B2", "C1", "C2"]


def compact(entry: dict) -> dict:
    senses = entry["senses"][:2]
    # The everyday example (index 0) is shorter and fits the screen better.
    ex = entry["ex"][0] if entry["ex"] else {"en": "", "zh": ""}
    return {
        "id": entry["id"],
        "w": entry["w"],
        "ipa": entry["us"] or entry["uk"],
        "zh": "；".join(f"{s['pos']} {s['zh']}" for s in senses),
        "ex": ex["en"],
        "exZh": ex["zh"],
        "cefr": entry["cefr"],
    }


def pick(words: list[dict], limit: int, cefr: str | None, topic: str | None) -> list[dict]:
    pool = [w for w in words if (not cefr or w["cefr"] == cefr) and (not topic or w["topic"] == topic)]
    pool.sort(key=lambda w: (CEFR.index(w["cefr"]), w["id"]))
    return pool[:limit] if limit else pool


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--limit", type=int, default=50, help="number of words, 0 for all")
    ap.add_argument("--cefr", choices=CEFR)
    ap.add_argument("--topic")
    ap.add_argument("--size", type=int, default=240, help="illustration edge in px")
    ap.add_argument("--accent", choices=["us", "uk"], default="us")
    ap.add_argument("--out", type=Path, default=ROOT / "device" / "pack")
    args = ap.parse_args()

    words = json.loads((ROOT / "data" / "word-atlas.json").read_text(encoding="utf-8"))
    chosen = pick(words, args.limit, args.cefr, args.topic)
    if not chosen:
        raise SystemExit("no words match the filters")

    out: Path = args.out
    if out.exists():
        shutil.rmtree(out)
    (out / "img").mkdir(parents=True)
    (out / "audio").mkdir()

    atlas = ROOT / "public" / "atlas"
    entries, chars, missing = [], set(), []
    for w in chosen:
        item = compact(w)
        src_img = atlas / "img" / f"{w['id']}.webp"
        src_audio = atlas / "audio" / args.accent / f"{w['id']}.mp3"
        if src_img.exists():
            with Image.open(src_img) as im:
                im.convert("RGB").resize((args.size, args.size), Image.LANCZOS).save(
                    out / "img" / f"{w['id']}.jpg", "JPEG", quality=82, progressive=False
                )
            item["img"] = f"img/{w['id']}.jpg"
        else:
            missing.append(str(src_img.relative_to(ROOT)))
        if src_audio.exists():
            shutil.copyfile(src_audio, out / "audio" / f"{w['id']}.mp3")
            item["audio"] = f"audio/{w['id']}.mp3"
        else:
            missing.append(str(src_audio.relative_to(ROOT)))
        entries.append(item)
        chars.update(c for c in item["zh"] + item["exZh"] if ord(c) > 0x7F)

    (out / "words.json").write_text(json.dumps(entries, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    # The firmware reads this tab-separated copy so it needs no JSON parser.
    cols = ["id", "w", "ipa", "zh", "ex", "exZh", "img", "audio"]
    tsv = "".join("\t".join(str(e.get(c, "")).replace("\t", " ").replace("\n", " ") for c in cols) + "\n" for e in entries)
    (out / "words.tsv").write_text(tsv, encoding="utf-8")
    # Characters the on-device font has to cover, for subsetting a CJK font.
    (out / "charset.txt").write_text("".join(sorted(chars)), encoding="utf-8")

    size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
    print(f"{len(entries)} words, {len(chars)} CJK chars, {size / 1024:.0f} KiB -> {out}")
    for m in missing:
        print(f"missing: {m}")


if __name__ == "__main__":
    main()
