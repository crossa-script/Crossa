# Crossa Release Workflow

Crossa CLI releases are created manually from GitHub Actions:

```text
GitHub -> Actions -> Release Crossa -> Run workflow
```

The selected branch must be `main`. The `version` input must be a semantic
version such as:

```text
0.1.0
```

The workflow publishes the release and tag as:

```text
v0.1.0
```

## Build

The release workflow reuses the existing CMake target:

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --config Release --target crossa --parallel
```

The standalone CLI archives contain the compiled `crossa` executable and do
not include Crossa source files.

## Installation

macOS ARM64 and Linux x86_64 use the standalone shell installer:

```sh
curl -fsSL https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.sh | bash
```

Install a specific version:

```sh
curl -fsSL https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.sh | bash -s -- 0.1.0
```

The installer downloads the matching GitHub Release archive, verifies it
against `SHA256SUMS`, and installs the compiled CLI into:

```text
~/.crossa/bin/crossa
```

The installer does not clone the repository, build Crossa locally, require
`sudo`, or modify shell profiles.

Windows x86_64 uses the standalone PowerShell installer:

```powershell
irm https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.ps1 | iex
```

Install a specific Windows version:

```powershell
Invoke-WebRequest https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.ps1 -OutFile install.ps1
.\install.ps1 -Version 0.1.0
```

The Windows installer downloads the matching release zip, verifies it against
`SHA256SUMS`, and installs the compiled CLI into:

```text
%USERPROFILE%\.crossa\bin\crossa.exe
```

## Assets

Required release assets are:

```text
crossa-v0.1.0-macos-arm64.tar.gz
crossa-v0.1.0-linux-x86_64.tar.gz
crossa-v0.1.0-windows-x86_64.zip
SHA256SUMS
```

Windows x86_64 is built on GitHub-hosted Windows runners with the existing
CMake target and vcpkg-provided static libcurl.
