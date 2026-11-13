# Releasing mods

Release notes are short: one sentence per mod and SHA-256 for every ZIP.

## Publish

1. Merge changes into `main`.
2. Create a `vX.Y.Z` tag from the desired commit (GitHub **Releases → Draft a new release → Choose a tag**).
3. The **Publish mod releases** workflow packages all mods and publishes or updates that tag's GitHub release.

Regular commits do not run release builds. Issue/PR labeling runs only when those items change.

## New mods

Create a top-level mod folder containing installable files under `bin/`, `r6/`, `archive/`, `red4ext/`, `engine/`, `mods/`, or `dlc/`.
Every such folder is included automatically in the next release. Each ZIP contains game-root-relative paths.
Add an optional one-line name/summary to `.github/mod-descriptions.json` for nicer release notes.
The workflow includes only tracked install files and excludes runtime `db.json`.

## Verify a download

Windows PowerShell: `Get-FileHash .\No-Tracking-Please-v0.1.0.zip -Algorithm SHA256`.
Compare the result with the checksum in the release notes or `SHA256SUMS.txt`.

Releases do not prove that gameplay works: test compilation and in-game features before tagging a stable build.
