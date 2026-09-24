param(
    [ValidateSet('Debug', 'Profile', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
# Normal KChess development build. Native CMake/MSBuild intermediates are kept
# outside the repository under %LOCALAPPDATA% and reused automatically.
$projectRoot = Split-Path -Parent $PSScriptRoot
$flutterDir = Join-Path $projectRoot 'flutter_app'
$pubspec = Join-Path $flutterDir 'pubspec.yaml'
$packageConfig = Join-Path $flutterDir '.dart_tool\package_config.json'

if (-not (Test-Path -LiteralPath $flutterDir)) {
    throw "Flutter project not found: $flutterDir"
}

Push-Location $flutterDir
try {
    # flutter clean removes .dart_tool. Restore packages only when the package
    # config is missing or pubspec.yaml changed since it was generated.
    $needsPubGet = -not (Test-Path -LiteralPath $packageConfig)
    if (-not $needsPubGet) {
        $needsPubGet = (Get-Item -LiteralPath $pubspec).LastWriteTimeUtc -gt `
            (Get-Item -LiteralPath $packageConfig).LastWriteTimeUtc
    }

    if ($needsPubGet) {
        & flutter pub get
        if ($LASTEXITCODE -ne 0) {
            throw "flutter pub get failed with exit code $LASTEXITCODE"
        }
    }

    & flutter run -d windows --debug --no-pub
        if ($LASTEXITCODE -ne 0) {
            throw "flutter run -d windows failed with exit code $LASTEXITCODE"
        }
    }
finally {
    Pop-Location
}
