$RepoRoot = Split-Path -Parent $PSScriptRoot
$BinPath = Join-Path $RepoRoot "bin"
$env:PATH = "$BinPath;$env:PATH"
