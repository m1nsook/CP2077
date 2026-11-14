#!/usr/bin/env python3
"""Validate public, game-derived quest-ID mapping; optional local save comparison.

Never commit a player's metadata file or their finished quest list.
"""
import argparse
import csv
import json
from pathlib import Path


def validate(catalog: Path, quest_ids: Path, metadata: Path | None):
    catalog_codes = {}
    with catalog.open(encoding="utf-8", newline="") as stream:
        for row in csv.reader(stream, delimiter="\t"):
            if not row or row[0].startswith("#"):
                continue
            assert len(row) == 6, row
            h, category = int(row[0]), row[1]
            assert h not in catalog_codes
            catalog_codes[h] = category
    assert len(catalog_codes) == 212
    ids = {}
    seen_ids = set()
    with quest_ids.open(encoding="utf-8", newline="") as stream:
        for row in csv.reader(stream, delimiter="\t"):
            if not row or row[0].startswith("#"):
                continue
            assert len(row) == 2
            h, quest_id = int(row[0]), row[1]
            assert catalog_codes.get(h) in {"A", "O", "R", "C"}
            assert h not in ids and quest_id not in seen_ids
            assert quest_id.startswith("ma_")
            assert quest_id.replace("_", "").isalnum()
            seen_ids.add(quest_id)
            ids[h] = quest_id
    assert len(ids) == 175, f"expected 175 quest IDs; got {len(ids)}"
    assert sum(category == "T" for category in catalog_codes.values()) == 37
    print("PASS: 212 catalog entries, 175 unique quest ID mappings, 37 tarot (unmapped)")
    if metadata:
        payload = json.loads(metadata.read_text(encoding="utf-8"))["Data"]["metadata"]
        completed = set(payload["finishedQuests"].split())
        matched = [h for h, quest_id in ids.items() if quest_id in completed]
        print(f"Private local save comparison: {len(completed)} finished quest IDs, {len(matched)} catalog matches")
        if payload.get("gameVersion") != 2500:
            print("WARNING: metadata gameVersion differs from catalog build 2500")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    folder = root / "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins"
    parser.add_argument("--catalog", type=Path, default=folder / "catalog.tsv")
    parser.add_argument("--quest-ids", type=Path, default=folder / "quest_ids.tsv")
    parser.add_argument("--metadata", type=Path, help="optional private save metadata; do not commit")
    args = parser.parse_args()
    validate(args.catalog, args.quest_ids, args.metadata)
