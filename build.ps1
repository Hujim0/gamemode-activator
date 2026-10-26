# Requires MinGW-w64 gcc on PATH. Run from any directory.
param([string]$Compiler = "gcc")
$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force dist | Out-Null
    & $Compiler -Os -Wall -Wextra -Wpedantic -std=c11 -s -municode -mwindows `
        -o dist/gamestate-monitor.exe src/main.c src/detector.c -luser32
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
    Copy-Item gamemode_blacklist.txt dist/
    Get-Item dist/gamestate-monitor.exe
} finally {
    Pop-Location
}
