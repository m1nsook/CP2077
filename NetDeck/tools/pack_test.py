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
    "bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckDiagnostics/init.lua",
)
TESTING = """Infiltration — NetWatch Σ MK.1 / NETWORM SOURCE-ONLY TEST — NOT A RELEASE

Requires Cyberpunk 2077 2.5 + Phantom Liberty, RED4ext, redscript, TweakXL and Codeware.

INSTALL:
1. Back up the save you will use for testing and CLOSE THE GAME.
2. Extract this ZIP into the Cyberpunk 2077 game root.
3. Make sure r6/scripts/NetDeck/NetDeck.reds,
   r6/scripts/NetDeck/Localization.reds and r6/tweaks/NetDeck/NetDeck.yaml exist.
4. This diagnostic artifact contains only REDscript, TweakXL and a small
   CET diagnostic Lua script (no binary assets). The optional diagnostic
   script is bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckDiagnostics/init.lua.
   Advanced Crash Reporter reads its [NetDeck/DIAG] output from CET scripting.log.
   Original icon PNGs, WolvenKit raw project, .inkatlas, .xbm and compiled
   .archive are PRIVATE and NOT included. A separate locally packed archive
   containing both icon atlases is required for custom icons to display.
5. FIRST: Launch/load the SAME map or save that crashed previously. Do not
   use NETWORM or open its HUD yet. Test whether world loading is now stable.
   If a crash occurs, share the Advanced Crash Reporter .txt; it captures
   CET scripting.log, including [NetDeck/DIAG] session and heartbeat entries.
   If REDscript compilation or TweakXL loading fails, send the redscript
   and TweakXL errors; do not proceed to combat testing.
6. To add the experimental deck through CET console:
   Game.AddToInventory("Items.NESTCyberdeck", 1)
7. Equip NetWatch Σ MK.1; test NETWORM on a camera, terminal, isolated device,
   and a hostile NPC without requiring Ping. A camera is not required.
8. Confirm the original Ping is offered and works separately.
9. Test connected security effects without any camera in the network.
10. Check an unrelated network remains unaffected.
11. AFTER confirming stable loading: test ONE NETWORM upload against a gang.
    Several nearby enemies should get Tier 5 Sonic Shock at 2s intervals.
    Passive NPC and vehicle attach-based discovery was disabled for crash
    isolation, so spreading to targets not scanned may temporarily regress.
    Check a moving car, a road barrier and a non-quest door separately.
    Confirm the experimental native upload bar closes after a finished hack.
    The old "EXTRACTED DATA" reward notification is still present.
12. With Carhacker, verify NETWORM appears in the native vehicle scanner and
    one moving car upload brakes and spreads to a second loaded car.
13. Confirm ordinary NETWORM is not traced; NetWatch still triggers five stars.
14. Check €$ siphon totals and number of sources in the feedback.
15. Normal Netrunner XP is allowed. Check crafting components awarded.

IMPORTANT LIMITATIONS:
- v0.6.5 removes global NPCPuppet AND VehicleObject attach/detach wrappers
  to isolate repeated map-loading crashes after installing NetDeck.
  This is NOT compiler-verified and not a confirmed crash fix.
- CET logging is optional: it observes our upload/HUD methods (if available)
  and emits a 15s heartbeat without scanning entities or world streams.
  Advanced Crash Reporter 1.6 collects CET scripting.log automatically.
  If CET cannot observe a method, observer_unavailable is logged instead.
- Confirm map stability BEFORE trying hacks. If still crashing, remove NetDeck.
- Native HUD progress bar is experimental; no replacement red completion yet.
- Uses Phantom Liberty's QuickHack.BaseBlackWallHack record for the NPC action.
- Native Blackwall start/completion effects are cleared; confirm no lethal effect.
- Repeating Ping network grid audio/VFX and per-hop reward notifications are removed.
- Jumps run every 2.0 seconds and stop after at most 16 scheduled jumps.
- The TARGETS count is NPCs + moving cars with dispatched effects; device actions have their own count.
- Loaded hostile-NPC fallback now registers scanned or explicit-upload NPCs only,
  not every NPC on world attachment. Unscanned-NPC spread may regress.
- Previous build's network shutdown, vendor and stats were user-confirmed.
- Codeware interface-language packages cover 19 UI languages, with English fallback.
- No NETWORM access-point scanner actions. NPC/vehicle/door/network effects are experimental.
- No copyrighted or private icon/ArchiveXL/WolvenKit binary assets are distributed.
- NETWORM no longer inherits PingDevice, so vanilla Ping must remain
  an independent, functioning quickhack when NetWatch Σ MK.1 is installed.
- Timed PlayerPuppet wave, 2.0s per target, 4 hops of up to 25m, max 40
  affected loaded entities. Native network descendants also join the queue.
- Loaded cars are registered on native vehicle scanner actions and direct
  uploads, NOT global attach/detach. Unscanned car spread may regress.
- Dedicated VehicleQuickHack requires Carhacker and equipped NetWatch Σ MK.1.
- This is an unverified experimental build, not a confirmed gameplay fix.
- Eurodollar siphon is a capped gameplay reward; it does not drain actual
  NPC inventories. Normal Netrunner XP gain is allowed.
- Robotic NPC shutdown, drones and mechs are NOT yet covered.
- NETWORM does not need Camera Killer; test with or without it.
- Do not publish this ZIP as a Nexus Mods release.

REMOVE:
Delete r6/scripts/NetDeck and r6/tweaks/NetDeck, and optionally
bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckDiagnostics.
If you installed a separate local NetDeck.archive, disable it too.
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
    parser.add_argument("--output", type=Path, default=MOD / "dist/NetDeck-v0.6.5-SOURCE-TEST.zip")
    args = parser.parse_args()
    build(args.output)
