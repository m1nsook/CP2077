# Releasing mods

Release notes are short: one sentence per mod and SHA-256 for every ZIP.

## Publish

1. Merge changes into `main`.
2. Open **Actions → Release a mod → Run workflow** from `main`.
3. Choose one mod and enter a version such as `0.1.0` or `v0.1.0`.
4. The workflow publishes only the selected mod, with a game-root install ZIP, SHA-256 checksum, and mod-specific version tag.

Regular commits do not run release builds. Issue/PR labeling runs only when those items change.

## New mods

Create a top-level mod folder containing installable files under `bin/`, `r6/`, `archive/`, `red4ext/`, `engine/`, `mods/`, or `dlc/`.
Register each new mod in `.github/mod-descriptions.json` and the `release.yml` workflow choice list. Each ZIP contains game-root-relative paths.
Provide a one-line name/summary in `.github/mod-descriptions.json` for its release notes.
The workflow includes only tracked install files and excludes runtime `db.json`.

## Verify a download

Windows PowerShell: `Get-FileHash .\No-Tracking-Please-v0.1.0.zip -Algorithm SHA256`.
Compare the result with the checksum in the release notes or `SHA256SUMS.txt`.

Releases do not prove that gameplay works: test compilation and in-game features before tagging a stable build.
