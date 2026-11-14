#!/usr/bin/env python3
"""Validate exact, game-derived quest links for POI completion checks."""
from collections import Counter
from pathlib import Path
import argparse


def validate(catalog: Path, links: Path):
    categories = {}
    for line in catalog.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            parts = line.split("\t")
            assert len(parts) == 6
            h, category = int(parts[0]), parts[1]
            assert h not in categories
            categories[h] = category

    assert len(categories) == 212, f"expected 212 POIs; got {len(categories)}"
    found = {}
    counts = Counter()
    for line in links.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            parts = line.split("\t")
            assert len(parts) == 2, f"malformed quest link: {line}"
            h, path = int(parts[0]), parts[1]
            assert h in categories and h not in found, f"missing/duplicate hash {h}"
            cat = categories[h]
            if cat == "C":
                assert path.startswith("quests/minor_quest/ma_"), path
            else:
                assert cat in {"A", "O", "R"} and path.startswith("quests/minor_activities/"), path
            assert all(c.isalnum() or c in "_/" for c in path)
            found[h] = path
            counts[cat] += 1
    expected = Counter({"C": 17, "R": 26, "O": 14, "A": 2})
    assert counts == expected, f"expected {expected}, got {counts}"
    assert len(found) == 59
    print("PASS: 212 catalog entries, 59 linked quests, all 17 cyberpsychos linked")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    mod = root / "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins"
    parser.add_argument("--catalog", type=Path, default=mod / "catalog.tsv")
    parser.add_argument("--links", type=Path, default=mod / "quest_links.tsv")
    args = parser.parse_args()
    validate(args.catalog, args.links)
