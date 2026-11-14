#!/usr/bin/env python3
"""Validate the allowlisted 2.5 Night City marker catalog (no game assets)."""
import argparse
import collections
import csv
import json
import math
from pathlib import Path

KINDS = {
    "A": ("ncpd_assault", "GangWatchVariant", 114),
    "O": ("ncpd_organized_crime", "OutpostVariant", 18),
    "R": ("ncpd_reported_crime", "HiddenStashVariant", 26),
    "C": ("cyberpsycho", "HuntForPsychoVariant", 17),
    "T": ("tarot", "TarotVariant", 37),
    "F": ("fixer", "FixerVariant", 7),
}


def validate(path: Path, candidates: Path | None = None):
    rows = []
    known = set()
    counts = collections.Counter()
    with path.open(encoding="utf-8", newline="") as stream:
        for row in csv.reader(stream, delimiter="\t"):
            if not row or row[0].startswith("#"):
                continue
            assert len(row) == 6, f"wrong columns: {row}"
            h, cat, x, y, z, origin = row
            assert cat in KINDS, f"unknown category: {cat}"
            assert origin in {"b", "e"}, f"unknown origin: {origin}"
            h = int(h)
            x, y, z = float(x), float(y), float(z)
            assert 0 < h <= 0xFFFFFFFF and h not in known, f"duplicate/bad journal hash {h}"
            assert all(math.isfinite(v) for v in (x, y, z)), "nonfinite coordinate"
            assert abs(x) <= 10000 and abs(y) <= 10000 and abs(z) <= 5000, "out-of-bounds position"
            known.add(h)
            counts[cat] += 1
            rows.append((h, cat, x, y, z, origin))
    assert len(rows) == 219, f"expected 219 candidates; got {len(rows)}"
    assert counts == collections.Counter({key: props[2] for key, props in KINDS.items()}), counts
    for idx, a in enumerate(rows):
        for b in rows[idx + 1:]:
            if a[1] == b[1]:
                assert math.hypot(a[2] - b[2], a[3] - b[3]) >= 3, (
                    f"same-category coordinates too close: {a[0]} / {b[0]}"
                )
    if candidates:
        original = json.loads(candidates.read_text(encoding="utf-8"))["markers"]
        indexed = {int(p["journalHash"]): p for p in original}
        assert len(indexed) == len(rows), "candidate list is not 219 unique hashes"
        for h, cat, x, y, z, origin in rows:
            source = indexed[h]
            assert (source["category"], source["variant"]) == KINDS[cat][:2]
            assert source["origin"] == ("ep1" if origin == "e" else "base")
            assert all(abs(p - float(source[k])) < 0.000011 for k, p in (("x", x), ("y", y), ("z", z)))
    print(f"PASS: {len(rows)} allowlisted pins, no duplicates; categories {dict(counts)}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", type=Path, default=Path(__file__).parent.parent / "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins/catalog.tsv")
    ap.add_argument("--candidates", type=Path, help="optional offline 219-candidate JSON for exact comparison")
    args = ap.parse_args()
    validate(args.catalog, args.candidates)
