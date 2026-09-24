param(
    [int]$MaxFileSizeMB = 25
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$maxBytes = [int64]$MaxFileSizeMB * 1MB

function Get-CandidateFiles {
    Push-Location $repoRoot
    try {
        if (Get-Command git -ErrorAction SilentlyContinue) {
            $inside = (& git rev-parse --is-inside-work-tree 2>$null)
            if ($LASTEXITCODE -eq 0 -and $inside -eq 'true') {
                $paths = @()
                $paths += (& git ls-files)
                $paths += (& git ls-files --others --exclude-standard)
                return $paths | Where-Object { $_ } | Sort-Object -Unique | ForEach-Object {
                    $full = Join-Path $repoRoot $_
                    if (Test-Path -LiteralPath $full -PathType Leaf) { Get-Item -LiteralPath $full }
                }
            }
        }
        return Get-ChildItem -LiteralPath $repoRoot -File -Recurse -Force -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -notmatch '[\\/](\.git|build|\.dart_tool|\.gradle|CMakeFiles|third_party|secrets)[\\/]'
            }
    }
    finally {
        Pop-Location
    }
}

$violations = New-Object System.Collections.Generic.List[string]
$files = @(Get-CandidateFiles)

$privatePathPatterns = @(
    '(^|[\\/])secrets([\\/]|$)',
    '(^|[\\/])\.secrets([\\/]|$)',
    '(^|[\\/])credentials\.json$',
    '(^|[\\/])service-account[^\\/]*\.json$',
    '(^|[\\/])[^\\/]*credentials[^\\/]*\.json$',
    '(^|[\\/])\.env(?:\..+)?$',
    '(^|[\\/])token\.json$',
    '(^|[\\/])oauth[^\\/]*\.json$',
    '(^|[\\/])id_(?:rsa|ed25519)$'
)

$privateExtensions = @('.pem', '.p12', '.pfx', '.jks', '.keystore', '.key', '.ppk')
$modelExtensions = @('.pt', '.pth', '.onnx', '.kcep', '.kcep2', '.safetensors', '.ckpt')

foreach ($file in $files) {
    $relative = [IO.Path]::GetRelativePath($repoRoot, $file.FullName)

    foreach ($pattern in $privatePathPatterns) {
        if ($relative -match $pattern) {
            $violations.Add("private path: $relative")
            break
        }
    }

    if ($privateExtensions -contains $file.Extension.ToLowerInvariant()) {
        $violations.Add("private credential file: $relative")
    }

    if ($modelExtensions -contains $file.Extension.ToLowerInvariant()) {
        $violations.Add("local model binary: $relative")
    }

    if ($file.Length -gt $maxBytes) {
        $sizeMB = [Math]::Round($file.Length / 1MB, 1)
        $violations.Add("large file ${sizeMB}MB > ${MaxFileSizeMB}MB: $relative")
    }

    if ($file.Length -le 2MB -and $file.Extension.ToLowerInvariant() -in @('.txt','.md','.json','.yaml','.yml','.toml','.ini','.cfg','.cpp','.cc','.c','.h','.hpp','.dart','.py','.ps1','.cmake')) {
        try {
            $text = [IO.File]::ReadAllText($file.FullName)
            if ($text -match '-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----') {
                $violations.Add("embedded private key: $relative")
            }
            if ($text -match '(?<![A-Za-z0-9])AIza[0-9A-Za-z_-]{30,}') {
                $violations.Add("embedded Google API key candidate: $relative")
            }
            if ($text -match '(?<![A-Za-z0-9])sk-[A-Za-z0-9_-]{20,}') {
                $violations.Add("embedded API key candidate: $relative")
            }
        }
        catch {
            # Binary/encoding anomalies are ignored by this lightweight guard.
        }
    }
}

$violations = @($violations | Sort-Object -Unique)
if ($violations.Count -gt 0) {
    Write-Host "Repository hygiene FAILED ($($violations.Count) finding(s)):" -ForegroundColor Red
    $violations | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host "Repository hygiene OK: no visible secrets/model binaries and no files above ${MaxFileSizeMB}MB." -ForegroundColor Green
