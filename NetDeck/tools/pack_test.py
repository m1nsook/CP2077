#!/usr/bin/env python3
"""Build a standalone GAME-ROOT NΣT development ZIP (no publishing)."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

MOD = Path(__file__).resolve().parents[1]
ROOT_FILES = (
    "r6/scripts/NetDeck/NetDeck.reds",
    "r6/scripts/NetDeck/Localization.reds",
    "r6/tweaks/NetDeck/NetDeck.yaml",
)
TESTING = """Infiltration — NΣT GAMEPLAY TEST — NOT A RELEASE

Requires Cyberpunk 2077 2.5, RED4ext, redscript, TweakXL and Codeware.

INSTALL:
1. Back up the save you will use for testing and CLOSE THE GAME.
2. Extract this ZIP into the Cyberpunk 2077 game root.
3. Make sure r6/scripts/NetDeck/NetDeck.reds,
   r6/scripts/NetDeck/Localization.reds and r6/tweaks/NetDeck/NetDeck.yaml exist.
4. Launch the game. If REDscript compilation or TweakXL loading fails,
   send the r6/logs/redscript* and red4ext/plugins/TweakXL/TweakXL.log errors.
5. To add the experimental deck through CET console:
   Game.AddToInventory("Items.NESTCyberdeck", 1)
6. Equip NΣT; test Infiltration on a camera, terminal, isolated device,
   and pingable hostile NPC. A camera is NOT required as an entry point.
7. Confirm the original Ping is offered and works separately.
8. Test connected security effects without any camera in the network.
9. Check an unrelated network remains unaffected.

IMPORTANT LIMITATIONS:
- This regression fix is source/package tested but NOT reverified in game 2.5.
- Previous build's network shutdown, vendor and stats were user-confirmed.
- The custom NΣT and Infiltration names/descriptions use Codeware's native
  localization provider. The icon still uses an existing vanilla asset.
- Infiltration no longer inherits PingDevice, so vanilla Ping must remain
  an independent, functioning quickhack when NΣT is installed.
- NPCs are new entry points via PuppetQuickHack. Their connected devices
  are affected if they have a real network link. No link means no devices
  to shut down; we never invent nearby connections.
- Robotic NPC shutdown, drones and mechs are NOT yet covered.
- Infiltration does not need Camera Killer; test with or without it.
- Do not publish this ZIP as a Nexus Mods release.

REMOVE:
Delete r6/scripts/NetDeck and r6/tweaks/NetDeck.
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
        info = ZipInfo("NetDeck-READ-FIRST.txt", date_time=(2026, 11, 18, 0, 0, 0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        package.writestr(info, TESTING.encode("utf-8"))

    with ZipFile(output) as package:
        names = set(package.namelist())
        assert names == set(ROOT_FILES) | {"NetDeck-READ-FIRST.txt"}, names
        assert package.testzip() is None
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    sums = output.with_name("NetDeck-TEST-SHA256SUMS.txt")
    sums.write_text(f"{digest}  {output.name}\n", encoding="utf-8")
    print(f"Created {output} ({output.stat().st_size} bytes)")
    print(f"SHA-256: {digest}")
    print(f"Checksum: {sums}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=MOD / "dist/NetDeck-v0.4.0-TEST.zip")
    args = parser.parse_args()
    build(args.output)
