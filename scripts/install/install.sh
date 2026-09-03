#!/usr/bin/env bash

set -euo pipefail

CROSSA_REPOSITORY="crossa-script/Crossa"
CROSSA_HOME="${CROSSA_HOME:-${HOME}/.crossa}"
CROSSA_BIN_DIR="${CROSSA_HOME}/bin"

log() {
    printf '%s\n' "$1"
}

fail() {
    printf 'Crossa installation failed: %s\n' "$1" >&2
    exit 1
}

normalize_version() {
    local requested_version="$1"

    if [[ "${requested_version}" == v* ]]; then
        requested_version="${requested_version#v}"
    fi
    if [[ ! "${requested_version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        fail "Invalid Crossa release version: ${requested_version}"
    fi
    printf 'v%s' "${requested_version}"
}

detect_platform() {
    local os
    local arch

    os="$(uname -s)"
    arch="$(uname -m)"

    case "${os}" in
        Darwin)
            case "${arch}" in
                arm64)
                    printf 'macos-arm64'
                    ;;
                x86_64)
                    fail "Crossa does not currently provide a release for macOS x86_64."
                    ;;
                *)
                    fail "Crossa does not currently provide a release for macOS ${arch}."
                    ;;
            esac
            ;;

        Linux)
            case "${arch}" in
                x86_64|amd64)
                    printf 'linux-x86_64'
                    ;;
                arm64|aarch64)
                    fail "Crossa does not currently provide a release for Linux arm64."
                    ;;
                *)
                    fail "Crossa does not currently provide a release for Linux ${arch}."
                    ;;
            esac
            ;;

        *)
            fail "Crossa does not currently provide a release for ${os} ${arch}."
            ;;
    esac
}

resolve_version() {
    local requested_version="${1:-}"

    if [[ -n "${requested_version}" ]]; then
        normalize_version "${requested_version}"
        return
    fi

    local latest_url
    latest_url="$(
        curl \
            --fail \
            --silent \
            --show-error \
            --location \
            --output /dev/null \
            --write-out '%{url_effective}' \
            "https://github.com/${CROSSA_REPOSITORY}/releases/latest"
    )"

    local version
    version="${latest_url##*/}"

    [[ "${version}" == v* ]] ||
        fail "Could not resolve the latest Crossa release."

    printf '%s' "${version}"
}

verify_checksum() {
    local archive_path="$1"
    local checksums_path="$2"
    local archive_name="$3"
    local platform="$4"

    local expected
    expected="$(
        awk -v name="${archive_name}" '$2 == name { print $1 }' "${checksums_path}"
    )"
    [[ "${expected}" != *$'\n'* && "${expected}" =~ ^[[:xdigit:]]{64}$ ]] ||
        fail "Checksum manifest contains an invalid or duplicate entry for ${archive_name}."

    [[ -n "${expected}" ]] ||
        fail "Checksum for ${archive_name} was not found."

    local actual

    case "${platform}" in
        macos-*)
            command -v shasum >/dev/null 2>&1 ||
                fail "shasum is required to verify checksums."
            actual="$(shasum -a 256 "${archive_path}" | awk '{ print $1 }')"
            ;;
        linux-*)
            command -v sha256sum >/dev/null 2>&1 ||
                fail "sha256sum is required to verify checksums."
            actual="$(sha256sum "${archive_path}" | awk '{ print $1 }')"
            ;;
        *)
            fail "Cannot verify checksum for ${platform}."
            ;;
    esac

    [[ "${expected}" == "${actual}" ]] ||
        fail "Checksum verification failed for ${archive_name}."
}

install_crossa() {
    command -v curl >/dev/null 2>&1 ||
        fail "curl is required."

    command -v tar >/dev/null 2>&1 ||
        fail "tar is required."

    local platform
    platform="$(detect_platform)"

    local version
    version="$(resolve_version "${1:-}")"

    local package_name
    package_name="crossa-${version}-${platform}"

    local archive_name
    archive_name="${package_name}.tar.gz"

    local release_url
    release_url="https://github.com/${CROSSA_REPOSITORY}/releases/download/${version}"

    local temporary_directory
    temporary_directory="$(mktemp -d)"

    trap 'rm -rf "${temporary_directory}"' EXIT

    log "Installing Crossa ${version} for ${platform}..."

    curl \
        --fail \
        --silent \
        --show-error \
        --location \
        "${release_url}/${archive_name}" \
        --output "${temporary_directory}/${archive_name}"

    curl \
        --fail \
        --silent \
        --show-error \
        --location \
        "${release_url}/SHA256SUMS" \
        --output "${temporary_directory}/SHA256SUMS"

    verify_checksum \
        "${temporary_directory}/${archive_name}" \
        "${temporary_directory}/SHA256SUMS" \
        "${archive_name}" \
        "${platform}"

    mkdir -p "${temporary_directory}/extracted"

    while IFS= read -r archive_member; do
        case "${archive_member}" in
            "${package_name}/"|"${package_name}/crossa") ;;
            *) fail "Archive contains an unexpected or unsafe member: ${archive_member}" ;;
        esac
    done < <(tar -tzf "${temporary_directory}/${archive_name}")

    tar \
        -xzf "${temporary_directory}/${archive_name}" \
        -C "${temporary_directory}/extracted"

    local source_binary
    source_binary="${temporary_directory}/extracted/${package_name}/crossa"

    [[ -f "${source_binary}" && ! -L "${source_binary}" ]] ||
        fail "Crossa executable was not found inside ${archive_name}."
    [[ "$(wc -c < "${source_binary}")" -le 268435456 ]] ||
        fail "Crossa executable exceeds the configured extraction limit."

    mkdir -p "${CROSSA_BIN_DIR}"

    local temporary_binary
    temporary_binary="${CROSSA_BIN_DIR}/.crossa-install"

    cp "${source_binary}" "${temporary_binary}"
    chmod +x "${temporary_binary}"
    mv "${temporary_binary}" "${CROSSA_BIN_DIR}/crossa"

    log ""
    log "Crossa ${version} installed successfully."
    log "Installed at: ${CROSSA_BIN_DIR}/crossa"

    case ":${PATH}:" in
        *":${CROSSA_BIN_DIR}:"*)
            log ""
            log "Run:"
            log "  crossa doctor"
            ;;
        *)
            log ""
            log "Add Crossa to PATH:"
            log "  export PATH=\"${CROSSA_BIN_DIR}:\$PATH\""
            log ""
            log "Then run:"
            log "  crossa doctor"
            ;;
    esac
}

install_crossa "${1:-}"
