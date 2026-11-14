#!/usr/bin/env python3
"""Local tests for the manual single-mod release packager (not a GitHub workflow)."""
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_release import build  # noqa: E402


class OneModReleaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.previous = Path.cwd()
        os.chdir(self.tmp.name)
        subprocess.check_call(["git", "init", "-q"])
        self.make_file(".github/mod-descriptions.json", json.dumps({
            "UndiscoverMe": {"name": "UndiscoverMe", "summary": "Core mod."},
            "UndiscoverMe_Teleporter": {
                "name": "UndiscoverMe Teleporter", "summary": "Addon.", "requires": ["UndiscoverMe"]
            },
        }))
        self.make_file("UndiscoverMe/r6/scripts/UndiscoverMe/core.reds", "core")
        self.make_file("UndiscoverMe_Teleporter/r6/scripts/UDMT/addon.reds", "addon")
        self.make_file("UndiscoverMe_Teleporter/bin/x64/plugins/cet/init.lua", "lua")
        self.make_file("UndiscoverMe_Teleporter/bin/x64/plugins/cet/db.json", "private save")
        self.make_file("UndiscoverMe_Teleporter/readme.md", "not installed")
        subprocess.check_call(["git", "add", "-A"])
        self.make_file("UndiscoverMe_Teleporter/r6/scripts/UDMT/untracked.reds", "not shipped")

    def make_file(self, name, content):
        path = Path(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def tearDown(self):
        os.chdir(self.previous)
        self.tmp.cleanup()

    def test_one_mod_only_and_player_db_excluded(self):
        zip_file = build("UndiscoverMe_Teleporter", "v0.1.0-dev.1", Path("dist"))
        with ZipFile(zip_file) as archive:
            self.assertEqual(archive.namelist(), [
                "bin/x64/plugins/cet/init.lua", "r6/scripts/UDMT/addon.reds"
            ])
            self.assertEqual(archive.read("r6/scripts/UDMT/addon.reds"), b"addon")
        self.assertEqual(Path("dist/RELEASE_TAG.txt").read_text().strip(), "undiscoverme-teleporter-v0.1.0-dev.1")
        self.assertIn("Requires:** UndiscoverMe", Path("dist/RELEASE_NOTES.md").read_text())
        checksums = Path("dist/SHA256SUMS.txt").read_text().strip().split()
        self.assertEqual(checksums[0], hashlib.sha256(zip_file.read_bytes()).hexdigest())
        self.assertEqual(checksums[1], zip_file.name)
        self.assertEqual(list(Path("dist").glob("*.zip")), [zip_file])

    def test_core_only(self):
        path = build("UndiscoverMe", "v1.0.0", Path("dist"))
        with ZipFile(path) as archive:
            self.assertEqual(archive.namelist(), ["r6/scripts/UndiscoverMe/core.reds"])
        self.assertEqual(Path("dist/RELEASE_TAG.txt").read_text().strip(), "undiscoverme-v1.0.0")

    def test_reject_unknown_mod_or_invalid_version(self):
        for mod, version in [("OtherMod", "v0.1.0"), ("UndiscoverMe", "v0.1.0;echo hacked"), ("UndiscoverMe", "../v0.1.0")]:
            with self.subTest(mod=mod, version=version):
                with self.assertRaises(ValueError):
                    build(mod, version, Path("dist"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
