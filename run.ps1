param(
    [string]$Config = "Debug"
)

$exe = Get-ChildItem -Path build -Recurse -Filter "BitMusic.exe" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match [regex]::Escape($Config) } |
    Select-Object -First 1

if (-not $exe) {
    $exe = Get-ChildItem -Path build -Recurse -Filter "BitMusic.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
}

if (-not $exe) {
    Write-Error "Executable not found. Run build.ps1 first."
    exit 1
}


# The exe finds assets/ and lang/ in the source folder while developing, or next to itself
# (where CMake copies them post-build).
Push-Location $exe.DirectoryName
try {
    & $exe.FullName
} finally {
    Pop-Location
}
