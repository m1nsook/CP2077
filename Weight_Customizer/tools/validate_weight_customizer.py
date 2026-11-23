#!/usr/bin/env python3
"""Validate Weight Customizer against the M1 Arc 1.0.0 manifest contract.

This offline test is not a replacement for game REDscript compilation.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MOD_ID = "m1nsook.weight-customizer"
REGISTRY = ROOT / "red4ext/plugins/m1arc/mods" / MOD_ID
SCRIPT = ROOT / "r6/scripts/WeightCustomizer/WeightCustomizer.reds"
LOCALES = ("en_US", "pl_PL", "ko_KR")


def main():
    manifest = json.loads((REGISTRY / "mod.json").read_text(encoding="utf-8"))
    schema = json.loads((REGISTRY / "settings.json").read_text(encoding="utf-8"))
    assert manifest["format"] == 1
    assert manifest["id"] == REGISTRY.name == MOD_ID
    assert re.fullmatch(r"[A-Za-z0-9._-]{1,96}", MOD_ID)
    assert manifest["name"] == "Weight Customizer"
    assert manifest["scope"] == "Gameplay"
    assert isinstance(manifest["shortDesc"], str) and len(manifest["shortDesc"].encode("utf-8")) <= 180
    assert re.fullmatch(r"\d+\.\d+\.\d+", manifest["version"])
    assert manifest["requires"] == {"m1arc": "1.0.0"}

    assert isinstance(schema, list) and len(schema) == 2
    assert {row["id"] for row in schema} == {"enabled", "limit"}
    by_id = {row["id"]: row for row in schema}
    assert by_id["enabled"]["kind"] == "boolean"
    assert by_id["enabled"]["default"] is True
    assert by_id["limit"]["kind"] == "integer"
    assert (by_id["limit"]["min"], by_id["limit"]["max"], by_id["limit"]["step"], by_id["limit"]["default"]) == (50, 5000, 10, 500)

    for locale in LOCALES:
        path = REGISTRY / "language" / f"{locale}.json"
        translation = json.loads(path.read_text(encoding="utf-8"))
        assert translation["name"] and translation["shortDesc"]
        assert set(translation["settings"]) == {"enabled", "limit"}
        for setting_id in ("enabled", "limit"):
            display = translation["settings"][setting_id]
            assert all(isinstance(display[k], str) and display[k] for k in ("label", "description", "group"))
        assert not any(k.startswith("enabled.") or k.startswith("limit.") for k in translation)

    script = SCRIPT.read_text(encoding="utf-8")
    for required in (
        "module MisoWeightCustomizer",
        "class WeightCustomizerSystem extends ScriptableSystem",
        'M1Arc_GetBool("m1nsook.weight-customizer", "enabled")',
        'M1Arc_GetInt("m1nsook.weight-customizer", "limit")',
        "gamedataStatType.CarryCapacity",
        "gameStatModifierType.Additive",
        "stats.RemoveModifier(",
        "stats.AddModifier(",
        "DelayCallback(callback, 0.50, false)",
        "class WeightCustomizerTick extends DelayCallback",
    ):
        assert required in script, f"Missing script contract: {required}"
    for disallowed in (
        'M1Arc_GetBool("m1nsook.weight",',
        "RemoveAllModifiers(",
        "AddSavedModifier(",
        "M1Arc_RegisterJson(",
        "registerForEvent(",
        ".m1s",
        ".m1c",
    ):
        assert disallowed not in script, f"Unexpected legacy code: {disallowed}"
    print("PASS: Weight Customizer native schema, IDs, translations, and script integration")


if __name__ == "__main__":
    main()
