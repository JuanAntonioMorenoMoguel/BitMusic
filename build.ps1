param(
    [string]$Config = "Debug"
)

if (-not $env:VCPKG_ROOT) {
    Write-Error "VCPKG_ROOT is not set. Point it at your vcpkg checkout before building."
    exit 1
}

cmake --preset default
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build --config $Config
exit $LASTEXITCODE
