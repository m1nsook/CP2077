"""Verify every generated native row against original data, not just count."""
from pathlib import Path
import csv
root = Path(__file__).resolve().parents[1]
def rows(name):
    with (root / "tools/catalog" / name).open(encoding="utf-8") as f:
        return [r for r in csv.reader(f, delimiter="\t") if r and not r[0].startswith("#")]
catalog = rows("catalog.tsv")
ids = dict(rows("quest_ids.tsv"))
kinds = {"A": ("ncpd", "GangWatchVariant", "Assault in Progress"), "O": ("ncpd", "OutpostVariant", "Organized Crime"), "R": ("ncpd", "HiddenStashVariant", "Reported Crime"), "C": ("psycho", "HuntForPsychoVariant", "Cyberpsycho Sighting"), "T": ("tarot", "TarotVariant", "Tarot Card")}
src = (root / "r6/scripts/UndiscoverMe/UndiscoverMeCatalog.reds").read_text(encoding="utf-8")
assert len(catalog) == 212 and len(ids) == 175
for h,c,x,y,z,origin in catalog:
    key, variant, caption = kinds[c]
    ep1 = "true" if origin == "e" else "false"
    quest = ids.get(h, "")
    line = f'ArrayPush(rows, UDMCatalogRow.Make({h}u, "{key}", gamedataMappinVariant.{variant}, {x}, {y}, {z}, {ep1}, "{quest}", "{caption}"));'
    assert src.count(line) == 1, h
assert src.count("ArrayPush(rows, UDMCatalogRow.Make(") == len(catalog)
print("PASS: all 212 native rows match original TSV and 175 quest IDs")
