#!/usr/bin/env python3
"""Offline contract validation: no game or REDscript compiler is available in CI.

This verifies the installable manifest, schema, filenames, and REDscript
consumer calls; in-game gameplay semantics require a game test.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT/"red4ext/plugins/m1arc/mods/m1nsook.weight"
SCRIPT = ROOT/"r6/scripts/M1ArcCarryWeight/M1ArcCarryWeight.reds"


def main():
    manifest=json.loads((REGISTRY/"mod.json").read_text(encoding="utf-8"))
    schema=json.loads((REGISTRY/"settings.json").read_text(encoding="utf-8"))
    assert manifest["format"] == 1
    assert manifest["id"] == REGISTRY.name
    assert re.fullmatch(r"[A-Za-z0-9._-]+",manifest["id"])
    assert manifest["requires"]["m1arc"] == "0.1.0"
    assert re.fullmatch(r"\d+\.\d+\.\d+",manifest["version"])
    assert isinstance(schema,list) and len(schema) == 2
    assert {x["id"] for x in schema} == {"enabled","limit"}
    by_id={x["id"]:x for x in schema}
    assert by_id["enabled"]["kind"]=="boolean"
    assert by_id["enabled"]["default"] is True
    assert by_id["limit"]["kind"]=="integer"
    assert (by_id["limit"]["min"],by_id["limit"]["max"],by_id["limit"]["default"])==(50,5000,500)
    assert (REGISTRY/"language/en_US.json").exists()
    english=json.loads((REGISTRY/"language/en_US.json").read_text(encoding="utf-8"))
    example=json.loads((REGISTRY/"language/en_US.json.example").read_text(encoding="utf-8"))
    assert english==example
    assert english["mod.name"] == manifest["name"]
    assert set(english)=={"mod.name","enabled.label","enabled.description","limit.label","limit.description"}
    script=SCRIPT.read_text(encoding="utf-8")
    for expected in (
        'module MisoCarryWeight',
        'M1Arc_GetBool("m1nsook.weight", "enabled")',
        'M1Arc_GetInt("m1nsook.weight", "limit")',
        "gamedataStatType.CarryCapacity",
        "gameStatModifierType.Additive",
        "stats.RemoveModifier(",
        "stats.AddModifier(",
        "DelayCallback(callback, 0.50, false)",
    ):
        assert expected in script, f"Missing REDscript consumer: {expected}"
    for forbidden in ("M1Arc_RegisterJson(", "RemoveAllModifiers(", "AddSavedModifier(", "registerForEvent(", "cyber_engine_tweaks", ".m1s", ".m1c"):
        assert forbidden not in script, f"Forbidden legacy runtime: {forbidden}"
    print("PASS: Carry Weight native manifest, settings, translations, REDscript references")


if __name__=="__main__":
    main()
