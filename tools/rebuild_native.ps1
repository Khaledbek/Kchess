param(
    [ValidateSet('Debug', 'Profile', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$nativeDir = [IO.Path]::GetFullPath((Join-Path $projectRoot 'native')).TrimEnd([char[]]"\/")

if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    throw 'LOCALAPPDATA is not available; cannot resolve the KChess native build cache.'
}

# Match native/cmake/windows_build_cache_root.cmake exactly: normalized absolute
# native-source path -> lowercase -> SHA-256 -> first 16 hex characters.
$sourceKey = $nativeDir.Replace('\', '/').ToLowerInvariant()
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $hashBytes = $sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($sourceKey))
}
finally {
    $sha256.Dispose()
}
$checkoutId = (-join ($hashBytes | ForEach-Object { $_.ToString('x2') })).Substring(0, 16)
$checkoutCache = Join-Path $env:LOCALAPPDATA "KChess\build-cache\$checkoutId"
$nativeCacheRoot = Join-Path $checkoutCache 'kchess-core'
$nativeBuildDir = Join-Path $nativeCacheRoot 'windows-x64\v2'
$sourceManifestPath = Join-Path $checkoutCache 'native-source-manifest.json'

function Resolve-CMakeExecutable {
    $command = Get-Command 'cmake.exe' -ErrorAction SilentlyContinue
    if ($command -and (Test-Path -LiteralPath $command.Source)) {
        return $command.Source
    }

    # Flutter/CMake already records the exact executable path in CMakeCache.txt.
    # Reuse it when available instead of requiring CMake on the global PATH.
    $cacheCandidates = @(
        (Join-Path $projectRoot 'flutter_app\build\windows\x64\CMakeCache.txt'),
        (Join-Path $nativeBuildDir 'CMakeCache.txt')
    )
    foreach ($cacheFile in $cacheCandidates) {
        if (-not (Test-Path -LiteralPath $cacheFile)) {
            continue
        }
        $commandLine = Get-Content -LiteralPath $cacheFile | Where-Object {
            $_ -like 'CMAKE_COMMAND:INTERNAL=*'
        } | Select-Object -First 1
        if ($commandLine) {
            $candidate = $commandLine.Substring('CMAKE_COMMAND:INTERNAL='.Length)
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    # Visual Studio installs its own CMake even when cmake.exe is not added to
    # the shell PATH. Resolve the active installation through vswhere first.
    $vswhereCandidates = @()
    if (${env:ProgramFiles(x86)}) {
        $vswhereCandidates += (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe')
    }
    if ($env:ProgramFiles) {
        $vswhereCandidates += (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    }

    foreach ($vswhere in $vswhereCandidates | Select-Object -Unique) {
        if (-not (Test-Path -LiteralPath $vswhere)) {
            continue
        }
        $installations = & $vswhere -products '*' -all -property installationPath 2>$null
        foreach ($installation in $installations) {
            if ([string]::IsNullOrWhiteSpace($installation)) {
                continue
            }
            $candidate = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    # Final fallback for standard Visual Studio layouts if vswhere is missing.
    $vsRoots = @()
    if ($env:ProgramFiles) {
        $vsRoots += (Join-Path $env:ProgramFiles 'Microsoft Visual Studio')
    }
    if (${env:ProgramFiles(x86)}) {
        $vsRoots += (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio')
    }
    foreach ($root in $vsRoots | Select-Object -Unique) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }
        $candidate = Get-ChildItem -LiteralPath $root -Filter 'cmake.exe' -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like '*CommonExtensions*Microsoft*CMake*CMake*bin*cmake.exe' } |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    throw 'CMake executable not found. Install the Visual Studio C++ CMake tools or ensure cmake.exe is available to Flutter/Visual Studio.'
}

function Get-NativeInputFiles {
    $trackedExtensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl', '.cmake', '.def', '.rc')
    Get-ChildItem -LiteralPath $nativeDir -File -Recurse -ErrorAction Stop | Where-Object {
        $relativePath = $_.FullName.Substring($nativeDir.Length).TrimStart([char[]]"\/").Replace('\', '/')
        $isExcluded = $relativePath -like 'third_party/*' -or $relativePath -like 'prebuilt/*'
        $isBuildInput = $_.Name -eq 'CMakeLists.txt' -or $trackedExtensions -contains $_.Extension.ToLowerInvariant()
        (-not $isExcluded) -and $isBuildInput
    }
}

function Get-NativeSourceSnapshot([System.IO.FileInfo[]]$Files) {
    $snapshot = @{}
    foreach ($file in $Files) {
        $relativePath = $file.FullName.Substring($nativeDir.Length).TrimStart([char[]]"\/").Replace('\', '/')
        $snapshot[$relativePath] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $snapshot
}

function Read-NativeSourceManifest {
    $snapshot = @{}
    if (-not (Test-Path -LiteralPath $sourceManifestPath)) {
        return $snapshot
    }

    try {
        $manifest = Get-Content -LiteralPath $sourceManifestPath -Raw | ConvertFrom-Json
        if ($manifest.schema -ne 'kchess.native_source_manifest.v1') {
            return $snapshot
        }
        foreach ($entry in @($manifest.files)) {
            if ($entry.path -and $entry.sha256) {
                $snapshot[[string]$entry.path] = [string]$entry.sha256
            }
        }
    }
    catch {
        # A stale developer-only manifest must never block a native recovery build.
        return @{}
    }
    return $snapshot
}

function Write-NativeSourceManifest([hashtable]$Snapshot) {
    if (-not (Test-Path -LiteralPath $checkoutCache)) {
        New-Item -ItemType Directory -Path $checkoutCache -Force | Out-Null
    }
    $entries = @(
        $Snapshot.GetEnumerator() |
            Sort-Object Name |
            ForEach-Object {
                [pscustomobject]@{
                    path = $_.Key
                    sha256 = $_.Value
                }
            }
    )
    [pscustomobject]@{
        schema = 'kchess.native_source_manifest.v1'
        generatedAtUtc = [DateTime]::UtcNow.ToString('o')
        files = $entries
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $sourceManifestPath -Encoding UTF8
}

function Get-ChangedNativeInputs([System.IO.FileInfo[]]$Files, [hashtable]$CurrentSnapshot) {
    $previousSnapshot = Read-NativeSourceManifest
    $changes = New-Object System.Collections.Generic.List[string]

    if ($previousSnapshot.Count -gt 0) {
        foreach ($path in $CurrentSnapshot.Keys) {
            if (-not $previousSnapshot.ContainsKey($path) -or $previousSnapshot[$path] -ne $CurrentSnapshot[$path]) {
                $changes.Add($path)
            }
        }
        foreach ($path in $previousSnapshot.Keys) {
            if (-not $CurrentSnapshot.ContainsKey($path)) {
                $changes.Add("$path (deleted)")
            }
        }
        return @($changes | Sort-Object -Unique)
    }

    # First run after this script update: use the last successful kchess_core
    # artifact as a best-effort baseline. Later runs use the exact SHA-256
    # manifest above, which survives deletion of the core build directory.
    $previousArtifact = Get-ChildItem -LiteralPath $nativeBuildDir -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq 'kchess_core.dll' -or $_.Name -eq 'kchess_core.lib' } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($previousArtifact) {
        foreach ($file in $Files) {
            if ($file.LastWriteTimeUtc -gt $previousArtifact.LastWriteTimeUtc) {
                $relativePath = $file.FullName.Substring($nativeDir.Length).TrimStart([char[]]"\/").Replace('\', '/')
                $changes.Add($relativePath)
            }
        }
    }
    return @($changes | Sort-Object -Unique)
}

function Invoke-CMakeQuiet([string]$Phase, [string[]]$Arguments) {
    $output = @(& $cmakeExe @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        throw "Native CMake $Phase failed with exit code $exitCode"
    }

    # Keep warnings/errors visible, but hide the normal full-rebuild file spam.
    $important = @($output | Where-Object {
        $line = $_.ToString()
        $line -match '(?i)(warning\s+[A-Z]*[0-9]+|:\s*warning\b|:\s*error\b|fatal error|cmake warning|cmake error)'
    })
    if ($important.Count -gt 0) {
        Write-Host "Native $Phase diagnostics:"
        $important | ForEach-Object { Write-Host $_ }
    }
}

$cmakeExe = Resolve-CMakeExecutable
$nativeInputFiles = @(Get-NativeInputFiles)
$currentSourceSnapshot = Get-NativeSourceSnapshot -Files $nativeInputFiles
$changedNativeInputs = @(Get-ChangedNativeInputs -Files $nativeInputFiles -CurrentSnapshot $currentSourceSnapshot)

# Preserve the generator used by an existing KChess/Flutter CMake tree when
# possible. This keeps the direct native rebuild aligned with Flutter's Windows
# toolchain while still allowing the native cache to be reset independently.
$generator = $null
$generatorCaches = @(
    (Join-Path $nativeBuildDir 'CMakeCache.txt'),
    (Join-Path $projectRoot 'flutter_app\build\windows\x64\CMakeCache.txt')
)
foreach ($cacheFile in $generatorCaches) {
    if (-not (Test-Path -LiteralPath $cacheFile)) {
        continue
    }
    $generatorLine = Get-Content -LiteralPath $cacheFile | Where-Object {
        $_ -like 'CMAKE_GENERATOR:INTERNAL=*'
    } | Select-Object -First 1
    if ($generatorLine) {
        $candidateGenerator = $generatorLine.Substring('CMAKE_GENERATOR:INTERNAL='.Length)
        if ($candidateGenerator -like 'Visual Studio *') {
            $generator = $candidateGenerator
            break
        }
    }
}

if (-not $generator) {
    $capabilities = (& $cmakeExe -E capabilities | Out-String) | ConvertFrom-Json
    $generator = ($capabilities.generators | Where-Object {
        $_.name -like 'Visual Studio *'
    } | Select-Object -First 1).name
}

if ([string]::IsNullOrWhiteSpace($generator)) {
    throw 'No Visual Studio CMake generator is available for the native Windows rebuild.'
}

if (Test-Path -LiteralPath $nativeCacheRoot) {
    Remove-Item -LiteralPath $nativeCacheRoot -Recurse -Force
}

# Reconfigure and build only kchess_core. Do not invoke Flutter here: this
# command is intentionally a native-only recovery path. Stable SF18/SF19
# prebuilt libraries and their separate cache are not removed.
Write-Host "Configuring native kchess_core ($Configuration)..."
Invoke-CMakeQuiet -Phase 'configure' -Arguments @(
    '-S', $nativeDir,
    '-B', $nativeBuildDir,
    '-G', $generator,
    '-A', 'x64',
    '-DKCHESS_BUILD_TESTS=OFF',
    '-DKCHESS_WITH_STOCKFISH=ON',
    '-DKCHESS_PERSISTENT_WINDOWS_BUILD=ON'
)

Write-Host 'Rebuilding native kchess_core...'
Invoke-CMakeQuiet -Phase 'build' -Arguments @(
    '--build', $nativeBuildDir,
    '--config', $Configuration,
    '--target', 'kchess_core',
    '--parallel',
    '--', '/nologo', '/verbosity:minimal'
)

Write-NativeSourceManifest -Snapshot $currentSourceSnapshot

Write-Host ''
if ($changedNativeInputs.Count -gt 0) {
    Write-Host 'Changed native source/build inputs since the previous successful native build:'
    $changedNativeInputs | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host 'Changed native source/build inputs: none detected.'
}
Write-Host "Native kchess_core rebuild completed ($Configuration)."
