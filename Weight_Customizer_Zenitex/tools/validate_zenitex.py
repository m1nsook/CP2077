#!/usr/bin/env python3
"""Static guardrails for the Zenitex compatibility patch (not game compilation)."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
SCRIPT = ROOT / "r6/scripts/WeightCustomizer/WeightCustomizer.reds"
BASE = REPO / "Weight_Customizer/r6/scripts/WeightCustomizer/WeightCustomizer.reds"

def main():
    assert BASE.is_file(), "Weight Customizer must remain in the repo"
    original = BASE.read_text(encoding="utf-8")
    patch = SCRIPT.read_text(encoding="utf-8")
    assert patch != original
    for marker in ("class WeightCustomizerSystem extends ScriptableSystem",
                   'M1Arc_GetBool("m1nsook.weight-customizer", "enabled")',
                   'M1Arc_GetInt("m1nsook.weight-customizer", "limit")',
                   "t\"OutfitSlots.Back\"", "t\"AttachmentSlots.Torso\"", "gameEItemIDFlag.Preview",
                   'n"ZenitexBackpackCustomMod"', 'n"ZenitexBackpackCustomModIconic"',
                   "this.UpdateZenitex(player, stats, currentEntity)",
                   "this.RemoveZenitexModifier()", "this.m_zenitexBonus"):
        assert marker in patch, marker
    assert "let wanted = Cast<Float>(target) + this.m_zenitexBonus + this.m_zenitexVanillaBonus;" in patch
    assert "RemoveAllModifiers(" not in patch
    assert not any(x in patch for x in (".m1s", ".m1c", "CET"))
    assert len(list(ROOT.glob("r6/**/*.reds"))) == 1, "Only replace the Weight Customizer script"
    print("PASS: original Weight Customizer remains intact; addon uses owned modifiers and preview-slot detection")

if __name__ == "__main__":
    main()
