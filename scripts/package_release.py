#!/usr/bin/env python3
"""Build one installable ZIP per mod and short release notes with SHA-256."""
import argparse
import hashlib
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path, PurePosixPath
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

INSTALL_DIRS = {"bin", "r6", "archive", "red4ext", "engine", "mods", "dlc"}


def tracked_mods():
    names = subprocess.check_output(["git", "ls-files", "-z", "--cached"]).decode("utf-8").split("\0")
    found = defaultdict(list)
    for name in names:
        if not name:
            continue
        path = PurePosixPath(name)
        if len(path.parts) < 3 or path.parts[1] not in INSTALL_DIRS:
            continue
        if path.name == "db.json" or path.name == ".gitkeep" or path.suffix in {".log", ".bak", ".tmp"}:
            continue
        if not Path(name).is_file():
            raise FileNotFoundError(f"Tracked file is missing: {name}")
        found[path.parts[0]].append(path)
    return {root: sorted(files, key=str) for root, files in sorted(found.items())}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True)
    ap.add_argument("--output", default="dist")
    args = ap.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.+-]+)?", args.tag):
        ap.error("Tag must look like v0.1.0 or v0.2.0-beta.1")
    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=True)
    descriptions = json.loads(Path(".github/mod-descriptions.json").read_text("utf-8"))
    mods = tracked_mods()
    if not mods:
        raise RuntimeError("No root-level directories with installable mod files found")

    overview, hashes, used_names = [], [], set()
    for directory, files in mods.items():
        zip_name = re.sub(r"[^A-Za-z0-9._-]", "-", directory).replace("_", "-") + "-" + args.tag + ".zip"
        if zip_name in used_names:
            raise RuntimeError(f"Duplicate ZIP name: {zip_name}")
        used_names.add(zip_name)
        zip_path = out / zip_name
        with ZipFile(zip_path, "w", ZIP_DEFLATED, compresslevel=9) as zf:
            for source in files:
                inside = PurePosixPath(*source.parts[1:]).as_posix()
                info = ZipInfo(inside, date_time=(2020, 1, 1, 0, 0, 0))
                info.compress_type = ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                zf.writestr(info, Path(str(source)).read_bytes(), compress_type=ZIP_DEFLATED, compresslevel=9)
        meta = descriptions.get(directory, {})
        name = meta.get("name", directory.replace("_", " "))
        summary = meta.get("summary", "Cyberpunk 2077 mod.")
        if "\n" in name or "\n" in summary:
            raise ValueError("Release metadata must be single-line")
        overview.append(f"- **{name}** — {summary}")
        hashes.append(f"{sha256(zip_path)}  {zip_name}")
        print(f"{name}: {len(files)} files -> {zip_name}")

    (out / "SHA256SUMS.txt").write_text("\n".join(hashes) + "\n", encoding="utf-8")
    notes = (
        "**Game:** Cyberpunk 2077 2.5\n\n"
        "### Included mods\n" + "\n".join(overview) +
        "\n\n### SHA-256\n"
        + "\n" + chr(96) * 3 + "text\n"
        + "\n".join(hashes)
        + "\n" + chr(96) * 3 + "\n"
    )
    (out / "RELEASE_NOTES.md").write_text(notes, encoding="utf-8")
    print(f"Packaged {len(mods)} mods; checksums and release notes written.")


if __name__ == "__main__":
    main()
