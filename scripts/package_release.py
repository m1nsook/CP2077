#!/usr/bin/env python3
"""Package exactly one Cyberpunk 2077 mod for a standalone GitHub Release."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path, PurePosixPath
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

INSTALL_DIRS = {"bin", "r6", "archive", "red4ext", "engine", "mods", "dlc"}
VERSION = re.compile(r"v\d+\.\d+\.\d+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?\Z")
IGNORED_EXTENSIONS = {".log", ".bak", ".tmp"}
IGNORED_NAMES = {"db.json", ".gitkeep"}


def tracked_install_files(mod: str) -> list[PurePosixPath]:
    """Use the Git index; never package untracked player saves/configuration."""
    output = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--", f"{mod}/"]
    ).decode("utf-8")
    paths = []
    for name in output.split("\0"):
        if not name:
            continue
        path = PurePosixPath(name)
        if (
            len(path.parts) < 3
            or path.parts[0] != mod
            or path.parts[1] not in INSTALL_DIRS
            or path.name in IGNORED_NAMES
            or path.suffix.lower() in IGNORED_EXTENSIONS
        ):
            continue
        if not Path(name).is_file():
            raise FileNotFoundError(f"Tracked install file is missing: {name}")
        paths.append(path)
    return sorted(paths, key=str)


def tracked_source_files(mod: str) -> list[PurePosixPath]:
    """A non-installable translator ZIP, preserving only checked-in source files."""
    output = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--", f"{mod}/"]
    ).decode("utf-8")
    result = []
    for name in output.split("\0"):
        if not name:
            continue
        path = PurePosixPath(name)
        if len(path.parts) < 2 or path.parts[0] != mod or path.name in IGNORED_NAMES:
            continue
        if not Path(name).is_file():
            raise FileNotFoundError(f"Tracked translation source file is missing: {name}")
        result.append(path)
    return sorted(result, key=str)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as src:
        for chunk in iter(lambda: src.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build(mod: str, version: str, output: Path) -> Path:
    if not VERSION.fullmatch(version):
        raise ValueError("Version must be like v0.1.0 or v0.2.0-dev.1")

    descriptions = json.loads(Path(".github/mod-descriptions.json").read_text("utf-8"))
    if mod not in descriptions:
        raise ValueError(f"Unknown mod {mod!r}. Choose a listed mod.")
    metadata = descriptions[mod]
    name = metadata["name"]
    summary = metadata["summary"]
    if not all(isinstance(v, str) and v and "\n" not in v for v in (name, summary)):
        raise ValueError("Mod name and summary must be nonempty, single-line strings")

    is_sources = metadata.get("package_type") == "sources"
    files = tracked_source_files(mod) if is_sources else tracked_install_files(mod)
    if not files:
        raise ValueError(f"No tracked installable files in {mod}/")
    output.mkdir(parents=True, exist_ok=True)
    zip_filename = mod.replace("_", "-") + "-" + version + ".zip"
    zip_path = output / zip_filename
    with ZipFile(zip_path, "w", compression=ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files:
            installed_path = PurePosixPath(*path.parts[1:]).as_posix()
            info = ZipInfo(installed_path, date_time=(2020, 1, 1, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, Path(str(path)).read_bytes(), compress_type=ZIP_DEFLATED, compresslevel=9)

    digest = sha256(zip_path)
    release_tag = mod.lower().replace("_", "-") + "-" + version
    requirements = metadata.get("requires", [])
    if not isinstance(requirements, list) or any(req not in descriptions for req in requirements):
        raise ValueError("Invalid mod dependency metadata")
    required_line = (
        "**Requires:** " + ", ".join(descriptions[req]["name"] for req in requirements) + ".\n\n"
        if requirements else ""
    )
    install = (
        "**Type:** Translator source archive. Do NOT extract into the game directory.\n\n"
        if is_sources else
        "**Install:** Extract the ZIP into your Cyberpunk 2077 game directory. "
        "The archive contains game-ready folders only; no tools, docs, or saved settings.\n\n"
    )
    notes = (
        f"## {name} {version}\n\n"
        f"{summary}\n\n"
        f"{required_line}"
        "**Game:** Cyberpunk 2077 2.5\n\n"
        f"{install}"
        "### SHA-256\n\n"
        f"{chr(96)}{digest}  {zip_filename}{chr(96)}\n"
    )
    (output / "SHA256SUMS.txt").write_text(f"{digest}  {zip_filename}\n", encoding="utf-8")
    (output / "RELEASE_NOTES.md").write_text(notes, encoding="utf-8")
    (output / "RELEASE_TAG.txt").write_text(release_tag + "\n", encoding="utf-8")
    (output / "RELEASE_TITLE.txt").write_text(f"{name} {version}\n", encoding="utf-8")
    print(f"{name}: {len(files)} files -> {zip_path} ({digest})")
    print(f"Release tag: {release_tag}")
    return zip_path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mod", required=True, help="One mod directory, e.g. UndiscoverMe_Teleporter")
    parser.add_argument("--tag", required=True, help="Mod version, e.g. v0.1.0")
    parser.add_argument("--output", default="dist")
    args = parser.parse_args()
    try:
        build(args.mod, args.tag, Path(args.output))
    except (ValueError, FileNotFoundError) as err:
        parser.error(str(err))


if __name__ == "__main__":
    main()
