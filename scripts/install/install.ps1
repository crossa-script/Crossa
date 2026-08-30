param(
    [string]$Version = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail {
    param([string]$Message)

    Write-Error "Crossa installation failed: $Message"
    exit 1
}

function Resolve-CrossaPlatform {
    $architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture

    switch ($architecture) {
        "X64" {
            Fail "Crossa does not currently provide a release for Windows x86_64."
        }

        "Arm64" {
            Fail "Crossa does not currently provide a release for Windows arm64."
        }

        default {
            Fail "Crossa does not currently provide a release for Windows $architecture."
        }
    }
}

function Install-Crossa {
    Resolve-CrossaPlatform | Out-Null
    Fail "Crossa does not currently provide Windows release artifacts."
}

Install-Crossa
