<#
  redscript CLI v1.0.1 checks, pinned to an upstream SHA-256.
  Parser is always run and fails CI on any syntax error.
  Full semantic lint + compile requires a privately supplied vanilla
  final.redscripts, since CDPR's compiled game scripts cannot be bundled in
  a public open-source repository.
#>
param(
  [Parameter(Mandatory=$true)]
  [string[]] $SourceRoots
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$release = "v1.0.1"
$expectedSha = "5e6dfcc914a41e300dac15cef245a84e5f881e1e3225265ebebb732e684a1717"
$toolDir = Join-Path $PWD ".ci/redscript"
New-Item -ItemType Directory -Force -Path $toolDir | Out-Null
$exe = Join-Path $toolDir "redscript-cli.exe"
$uri = "https://github.com/jac3km4/redscript/releases/download/$release/redscript-cli.exe"

Write-Host "Downloading pinned jac3km4/redscript CLI $release"
Invoke-WebRequest -Uri $uri -OutFile $exe
$actualSha = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha -ne $expectedSha) {
  throw "redscript-cli.exe SHA-256 mismatch: $actualSha"
}

foreach ($root in $SourceRoots) {
  if (!(Test-Path -LiteralPath $root)) {
    throw "REDscript source root not found: $root"
  }
  Write-Host "REDscript parser (real upstream CLI): $root"
  & $exe format -s $root
  if ($LASTEXITCODE -ne 0) { throw "REDscript parse failed: $root" }
}

$summary = $env:GITHUB_STEP_SUMMARY
if ($summary) {
  Add-Content $summary "### REDscript CLI $release"
  Add-Content $summary "- Real upstream source parsing: **passed**."
}

# Requires *both* secrets set on the repository. Never place the vanilla
# final.redscripts bundle in a public tree or publish compiler output.
$bundleUrl = $env:REDSCRIPT_GAME_BUNDLE_URL
$bundleSha = $env:REDSCRIPT_GAME_BUNDLE_SHA256
if ([string]::IsNullOrWhiteSpace($bundleUrl) -and [string]::IsNullOrWhiteSpace($bundleSha)) {
  Write-Warning "Semantic REDscript lint/compile SKIPPED: no private 2.5 vanilla final.redscripts supplied."
  if ($summary) {
    Add-Content $summary "- Semantic lint + compiler output: **not run — private game bundle not configured**."
  }
  exit 0
}
if ([string]::IsNullOrWhiteSpace($bundleUrl) -or [string]::IsNullOrWhiteSpace($bundleSha)) {
  throw "Configure BOTH REDSCRIPT_GAME_BUNDLE_URL and REDSCRIPT_GAME_BUNDLE_SHA256 secrets"
}
if (!$bundleUrl.StartsWith("https://", [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "REDSCRIPT_GAME_BUNDLE_URL must be HTTPS"
}
if ($bundleSha -notmatch '^[a-fA-F0-9]{64}$') {
  throw "REDSCRIPT_GAME_BUNDLE_SHA256 must be 64 hex digits"
}

$bundle = Join-Path $toolDir "final.redscripts"
Invoke-WebRequest -Uri $bundleUrl -OutFile $bundle
$actualBundleSha = (Get-FileHash $bundle -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualBundleSha -ne $bundleSha.ToLowerInvariant()) {
  throw "Vanilla REDscript bundle checksum mismatch"
}

$sourceArgs = @()
foreach ($root in $SourceRoots) { $sourceArgs += @("-s", $root) }

Write-Host "Running semantic REDscript lint against exact, verified vanilla 2.5 bundle"
& $exe lint @sourceArgs -b $bundle --runtime-compatible
if ($LASTEXITCODE -ne 0) { throw "REDscript semantic lint failed" }

$output = Join-Path $toolDir "compiled-check.redscripts"
Write-Host "Compiling game scripts with added mod scripts (no output published)"
& $exe compile @sourceArgs -b $bundle -o $output --runtime-compatible
if ($LASTEXITCODE -ne 0 -or !(Test-Path $output)) {
  throw "REDscript compile failed"
}
Remove-Item -Force -ErrorAction SilentlyContinue $output
Remove-Item -Force -ErrorAction SilentlyContinue $bundle
if ($summary) {
  Add-Content $summary "- Semantic lint against verified game 2.5 bundle: **passed**."
  Add-Content $summary "- Game script bundle compilation: **passed, generated output not uploaded**."
}
Write-Host "REDscript semantic lint and compilation passed."
