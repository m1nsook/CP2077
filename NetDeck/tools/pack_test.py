#!/usr/bin/env python3
"""Build a standalone game-root Infiltration development ZIP with no CET files."""
from __future__ import annotations
import argparse,hashlib
from pathlib import Path
from zipfile import ZIP_DEFLATED,ZipFile,ZipInfo

ROOT=Path(__file__).resolve().parents[1]
FILES=(
 "archive/pc/mod/NetDeck.archive",
 "r6/scripts/NetDeck/NetDeck.reds",
 "r6/scripts/NetDeck/NetDeckM1Arc.reds",
 "r6/scripts/NetDeck/NetDeckHudSkin.reds",
 "r6/scripts/NetDeck/Localization.reds",
 "r6/tweaks/NetDeck/NetDeck.yaml",
 "red4ext/plugins/m1arc/mods/m1nsook.infiltration/mod.json",
 "red4ext/plugins/m1arc/mods/m1nsook.infiltration/settings.json",
 "red4ext/plugins/m1arc/mods/m1nsook.infiltration/language/en_US.json",
 "red4ext/plugins/m1arc/mods/m1nsook.infiltration/language/pl_PL.json",
 "red4ext/plugins/m1arc/mods/m1nsook.infiltration/language/ko_KR.json",
)
def build(dest:Path)->None:
 dest.parent.mkdir(parents=True,exist_ok=True)
 with ZipFile(dest,"w",compression=ZIP_DEFLATED,compresslevel=9) as z:
  for name in FILES:
   content=(ROOT/name).read_bytes()
   info=ZipInfo(name,(2020,1,1,0,0,0))
   info.compress_type=ZIP_DEFLATED
   info.external_attr=0o100644<<16
   z.writestr(info,content)
 with ZipFile(dest) as z:
  assert set(z.namelist())==set(FILES)
  assert z.testzip() is None
 digest=hashlib.sha256(dest.read_bytes()).hexdigest()
 (dest.parent/"NetDeck-TEST-SHA256SUMS.txt").write_text(digest+"  "+dest.name+"\n",encoding="utf-8")
 print("Built",dest,"sha256",digest)

if __name__=="__main__":
 parser=argparse.ArgumentParser()
 parser.add_argument("--output",type=Path,default=Path("dist/NetDeck-v1.0.0-rc1.zip"))
 build(parser.parse_args().output)
