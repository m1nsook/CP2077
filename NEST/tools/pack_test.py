#!/usr/bin/env python3
"""Build a standalone GAME-ROOT NΣT development ZIP (no publishing)."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

MOD = Path(__file__).resolve().parents[1]
ROOT_FILES = (
    "r6/scripts/NEST/NEST.reds",
    "r6/tweaks/NEST/NEST.yaml",
)
TESTING = """NΣT — EXPERIMENTAL GAMEPLAY TEST — NOT A RELEASE

Requires Cyberpunk 2077 2.5, RED4ext, redscript and TweakXL.

INSTALL:
1. Back up the save you will use for testing and CLOSE THE GAME.
2. Extract this ZIP into the Cyberpunk 2077 game root.
3. Make sure r6/scripts/NEST/NEST.reds and r6/tweaks/NEST/NEST.yaml exist.
4. Launch the game. If REDscript compilation or TweakXL loading fails,
   send the r6/logs/redscript* and red4ext/plugins/TweakXL/TweakXL.log errors.
5. To add the experimental deck through CET console:
   Game.AddToInventory("Items.NESTCyberdeck", 1)
6. Check NΣT at ripperdocs, equip it, and scan a linked camera to see
   whether Infiltration is offered. Test its upload and network effect.
7. Check another network to confirm it was not affected.

IMPORTANT LIMITATIONS:
- Not verified in Cyberpunk 2077; further REDscript fixes may be needed.
- The custom NΣT name, Infiltration text and icon require a packed
  WolvenKit/ArchiveXL archive. The test build deliberately contains no
  .archive or .archive.xl file; missing names are EXPECTED.
- Robotic NPCs, drones and mechs are not yet covered.
- Infiltration does not need Camera Killer; test with or without it.
- Do not publish this ZIP as a Nexus Mods release.

REMOVE:
Delete r6/scripts/NEST and r6/tweaks/NEST.
""".replace("\n", "\n")

def build(output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(output, "w", compression=ZIP_DEFLATED, compresslevel=9) as package:
        for entry in ROOT_FILES:
            source = MOD / entry
            if not source.is_file():
                raise FileNotFoundError(source)
            data = source.read_bytes()
            info = ZipInfo(entry, date_time=(2026, 11, 18, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            package.writestr(info, data)
        info = ZipInfo("NEST-READ-FIRST.txt", date_time=(2026, 11, 18, 0, 0, 0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        package.writestr(info, TESTING.encode("utf-8"))

    with ZipFile(output) as package:
        names = set(package.namelist())
        assert names == set(ROOT_FILES) | {"NEST-READ-FIRST.txt"}, names
        assert package.testzip() is None
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    sums = output.with_name("NEST-TEST-SHA256SUMS.txt")
    sums.write_text(f"{digest}  {output.name}\n", encoding="utf-8")
    print(f"Created {output} ({output.stat().st_size} bytes)")
    print(f"SHA-256: {digest}")
    print(f"Checksum: {sums}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=MOD / "dist/NEST-v0.2.0-TEST.zip")
    args = parser.parse_args()
    build(args.output)
