param(
    [string]$Version = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$CrossaRepository = "crossa-script/Crossa"
$CrossaHome = if ($env:CROSSA_HOME) {
    $env:CROSSA_HOME
} else {
    Join-Path $HOME ".crossa"
}
$CrossaBinDirectory = Join-Path $CrossaHome "bin"

function Fail {
    param([string]$Message)

    Write-Error "Crossa installation failed: $Message"
    exit 1
}

function Resolve-CrossaPlatform {
    $architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture

    switch ($architecture) {
        "X64" {
            return "windows-x86_64"
        }

        "Arm64" {
            Fail "Crossa does not currently provide a release for Windows arm64."
        }

        default {
            Fail "Crossa does not currently provide a release for Windows $architecture."
        }
    }
}

function Resolve-CrossaVersion {
    param([string]$RequestedVersion)

    if ($RequestedVersion) {
        $normalizedVersion = $RequestedVersion.TrimStart("v")
        if ($normalizedVersion -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
            Fail "Invalid Crossa release version: $RequestedVersion"
        }
        return "v$normalizedVersion"
    }

    $latestUrl = "https://github.com/$CrossaRepository/releases/latest"

    try {
        $response = Invoke-WebRequest `
            -Uri $latestUrl `
            -MaximumRedirection 10 `
            -UseBasicParsing

        $finalUrl = $response.BaseResponse.ResponseUri.AbsoluteUri
        $tag = $finalUrl.TrimEnd("/").Split("/")[-1]
    }
    catch {
        Fail "Could not resolve the latest Crossa release."
    }

    if (-not $tag.StartsWith("v")) {
        Fail "Invalid latest Crossa release tag: $tag"
    }
    if ($tag.Substring(1) -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
        Fail "Invalid latest Crossa release tag: $tag"
    }

    return $tag
}

function Verify-CrossaChecksum {
    param(
        [string]$ArchivePath,
        [string]$ChecksumsPath,
        [string]$ArchiveName
    )

    $checksumLine = $null
    foreach ($line in Get-Content $ChecksumsPath) {
        $parts = $line -split "\s+"
        if ($parts.Length -ge 2 -and $parts[1] -eq $ArchiveName) {
            $checksumLine = $line
            break
        }
    }

    if (-not $checksumLine) {
        Fail "Checksum for $ArchiveName was not found."
    }

    $expectedChecksum = ($checksumLine -split "\s+")[0].ToLowerInvariant()
    $matchingLines = @(Get-Content $ChecksumsPath | Where-Object {
        $parts = $_ -split "\s+"
        $parts.Length -ge 2 -and $parts[1] -eq $ArchiveName
    })
    if ($matchingLines.Count -ne 1 -or $expectedChecksum -notmatch '^[0-9a-f]{64}$') {
        Fail "Checksum manifest contains an invalid or duplicate entry for $ArchiveName."
    }
    $actualChecksum = (
        Get-FileHash `
            -Algorithm SHA256 `
            -Path $ArchivePath
    ).Hash.ToLowerInvariant()

    if ($expectedChecksum -ne $actualChecksum) {
        Fail "Checksum verification failed for $ArchiveName."
    }
}

function Install-Crossa {
    $platform = Resolve-CrossaPlatform
    $resolvedVersion = Resolve-CrossaVersion -RequestedVersion $Version
    $packageName = "crossa-$resolvedVersion-$platform"
    $archiveName = "$packageName.zip"
    $releaseUrl = "https://github.com/$CrossaRepository/releases/download/$resolvedVersion"
    $temporaryDirectory = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ("crossa-" + [Guid]::NewGuid().ToString())

    New-Item `
        -ItemType Directory `
        -Path $temporaryDirectory `
        -Force | Out-Null

    try {
        $archivePath = Join-Path $temporaryDirectory $archiveName
        $checksumsPath = Join-Path $temporaryDirectory "SHA256SUMS"

        Write-Host "Installing Crossa $resolvedVersion for $platform..."

        Invoke-WebRequest `
            -Uri "$releaseUrl/$archiveName" `
            -OutFile $archivePath `
            -UseBasicParsing

        Invoke-WebRequest `
            -Uri "$releaseUrl/SHA256SUMS" `
            -OutFile $checksumsPath `
            -UseBasicParsing

        Verify-CrossaChecksum `
            -ArchivePath $archivePath `
            -ChecksumsPath $checksumsPath `
            -ArchiveName $archiveName

        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
        try {
            foreach ($entry in $zip.Entries) {
                $entryName = $entry.FullName.Replace('\', '/')
                if ($entryName -ne "$packageName/" -and
                    $entryName -ne "$packageName/crossa.exe") {
                    Fail "Archive contains an unexpected or unsafe member: $entryName"
                }
            }
        }
        finally {
            $zip.Dispose()
        }

        $extractDirectory = Join-Path $temporaryDirectory "extracted"

        Expand-Archive `
            -Path $archivePath `
            -DestinationPath $extractDirectory `
            -Force

        $sourceBinary = Join-Path $extractDirectory "$packageName/crossa.exe"

        $sourceItem = if (Test-Path $sourceBinary) { Get-Item $sourceBinary } else { $null }
        if (-not $sourceItem -or $sourceItem.PSIsContainer -or
            ($sourceItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            Fail "crossa.exe was not found inside $archiveName."
        }

        New-Item `
            -ItemType Directory `
            -Path $CrossaBinDirectory `
            -Force | Out-Null

        $destination = Join-Path $CrossaBinDirectory "crossa.exe"
        $temporaryDestination = Join-Path $CrossaBinDirectory ".crossa-install.exe"

        Copy-Item $sourceBinary $temporaryDestination -Force
        Move-Item $temporaryDestination $destination -Force

        Write-Host ""
        Write-Host "Crossa $resolvedVersion installed successfully."
        Write-Host "Installed at: $destination"

        $pathEntries = $env:PATH -split ";"
        if ($pathEntries -contains $CrossaBinDirectory) {
            Write-Host ""
            Write-Host "Run:"
            Write-Host "  crossa doctor"
        } else {
            Write-Host ""
            Write-Host "Add Crossa to PATH:"
            Write-Host "  `$env:Path = `"$CrossaBinDirectory;`$env:Path`""
            Write-Host ""
            Write-Host "Then run:"
            Write-Host "  crossa doctor"
        }
    }
    finally {
        if (Test-Path $temporaryDirectory) {
            Remove-Item `
                -Path $temporaryDirectory `
                -Recurse `
                -Force
        }
    }
}

Install-Crossa
