#!/usr/bin/env python3
"""Build a standalone GAME-ROOT NetWatch Σ MK.1 development ZIP (no publishing)."""
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
TESTING = """Infiltration — NetWatch Σ MK.1 / NETWORM SOURCE-ONLY TEST — NOT A RELEASE

Requires Cyberpunk 2077 2.5, RED4ext, redscript, TweakXL and Codeware.

INSTALL:
1. Back up the save you will use for testing and CLOSE THE GAME.
2. Extract this ZIP into the Cyberpunk 2077 game root.
3. Make sure r6/scripts/NetDeck/NetDeck.reds,
   r6/scripts/NetDeck/Localization.reds and r6/tweaks/NetDeck/NetDeck.yaml exist.
4. IMPORTANT: This GitHub artifact contains ONLY REDscript and TweakXL sources.
   Original icon PNGs, WolvenKit raw project, .inkatlas, .xbm and compiled
   .archive are PRIVATE and NOT included. A separate locally packed archive
   containing both icon atlases is required for custom icons to display.
5. Launch the game. If REDscript compilation or TweakXL loading fails,
   send the r6/logs/redscript* and red4ext/plugins/TweakXL/TweakXL.log errors.
6. To add the experimental deck through CET console:
   Game.AddToInventory("Items.NESTCyberdeck", 1)
7. Equip NetWatch Σ MK.1; test NETWORM on a camera, terminal, isolated device,
   and pingable hostile NPC. A camera is NOT required as an entry point.
8. Confirm the original Ping is offered and works separately.
9. Test connected security effects without any camera in the network.
10. Check an unrelated network remains unaffected.
11. Test one-upload multi-hop NETWORM spreading to multiple NPCs/vehicles/devices.
    Try an ordinary road blockade and a linked door; don't modify quest doors.
    Check that feedback device counts match dispatched device actions.
    Check that only the logged network devices appear in the feedback device count.
12. Verify NETWORM is untraceable on ordinary targets but triggers NetWatch five stars.
13. Check €$ siphon totals and number of sources in the feedback.
14. Watch for native XP on netrunner burnout or hacking and check components.

IMPORTANT LIMITATIONS:
- v0.6.0 corrects REDscript signature errors but is NOT game-compiler verified.
- Previous build's network shutdown, vendor and stats were user-confirmed.
- Codeware interface-language packages cover 19 UI languages, with English fallback.
- No NETWORM access-point scanner actions. NPC/vehicle/door/network effects are experimental.
- No copyrighted or private icon/ArchiveXL/WolvenKit binary assets are distributed.
- NETWORM no longer inherits PingDevice, so vanilla Ping must remain
  an independent, functioning quickhack when NetWatch Σ MK.1 is installed.
- NETWORM can jump up to 4 hops of 18m (max 64 visited world objects) and
  also follows actual connected device descendants. Proximity spread crosses
  native network boundaries by design; this is an unverified experimental build.
- Eurodollar siphon is a capped gameplay reward; it does not drain actual
  NPC inventories. Native XP suppression still needs implementation.
- Robotic NPC shutdown, drones and mechs are NOT yet covered.
- NETWORM does not need Camera Killer; test with or without it.
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
    parser.add_argument("--output", type=Path, default=MOD / "dist/NetDeck-v0.6.0-SOURCE-TEST.zip")
    args = parser.parse_args()
    build(args.output)
