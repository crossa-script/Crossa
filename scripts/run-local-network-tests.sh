#!/usr/bin/env bash

set -euo pipefail

if [[ "$#" -ne 1 ]]; then
    printf '%s\n' 'Usage: run-local-network-tests.sh <crossa>' >&2
    exit 1
fi
crossaBinary="$1"
scriptDirectory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
projectRoot="$(cd "$scriptDirectory/.." && pwd)"
stateDirectory="$(mktemp -d)"
serverOutput="$stateDirectory/server.log"
export CROSSA_LOCAL_NETWORK_STATE_DIRECTORY="$stateDirectory"

cleanup() {
    if [[ -n "${serverPid:-}" ]]; then
        kill "$serverPid" 2>/dev/null || true
        wait "$serverPid" 2>/dev/null || true
    fi
    rm -rf "$stateDirectory"
}

trap cleanup EXIT
if command -v socat >/dev/null 2>&1; then
    socat TCP-LISTEN:18765,bind=127.0.0.1,reuseaddr,fork EXEC:"/bin/bash $scriptDirectory/handle-local-network-request.sh" > "$serverOutput" 2>&1 &
elif command -v ruby >/dev/null 2>&1; then
    ruby "$scriptDirectory/local-network-ruby-server.rb" "$scriptDirectory/handle-local-network-request.sh" > "$serverOutput" 2>&1 &
else
    printf '%s\n' 'Test failed: socat or ruby is required for local network integration.' >&2
    exit 1
fi
serverPid="$!"
sleep 0.1
if ! kill -0 "$serverPid" 2>/dev/null; then
    printf '%s\n' 'Test failed: local network server did not start.' >&2
    exit 1
fi

fail() {
    printf '%s\n' "$1" >&2
    exit 1
}

runCase() {
    local fixture="$1"
    local expectedStatus="$2"
    local expectedText="$3"
    local debugEnabled="$4"
    local output
    local status
    local arguments=(test "$fixture")

    if [[ "$debugEnabled" == 'true' ]]; then
        arguments+=(--debug)
    fi
    if output="$("$crossaBinary" "${arguments[@]}" 2>&1)"; then
        status=0
    else
        status=$?
    fi
    if [[ "$status" -ne "$expectedStatus" ]]; then
        fail "$(basename "$fixture") returned $status: $output"
    fi
    if [[ -n "$expectedText" && "$output" != *"$expectedText"* ]]; then
        fail "$(basename "$fixture") did not contain '$expectedText': $output"
    fi
    printf '%s' "$output"
}

localOutput="$(runCase "$projectRoot/tests/local-network/request.cra" 0 '' true)"
networkLogs="$(printf '%s\n' "$localOutput" | grep 'Network request' || true)"
if [[ "$networkLogs" != *'headerValues='* ]]; then
    fail 'Enabled header logging did not appear.'
fi
if [[ "$networkLogs" != *'body='* ]]; then
    fail 'Enabled body logging did not appear.'
fi
if [[ "$networkLogs" == *'secret-token'* ]]; then
    fail 'Excluded Authorization header appeared in logs.'
fi
if [[ "$localOutput" != *'telemetry event=completed'* ]]; then
    fail 'Structured telemetry event did not appear.'
fi
if [[ "$localOutput" != *'streamed=true'* || "$localOutput" != *'downloadChunks='* ]]; then
    fail 'Transfer streaming metrics did not appear.'
fi
runCase "$projectRoot/tests/local-network/http-error.cra" 1 'http/http_status' false >/dev/null
runCase "$projectRoot/tests/local-network/invalid-json.cra" 1 'serialization/invalid_json' false >/dev/null
runCase "$projectRoot/tests/local-network/timeout.cra" 1 'timeout' false >/dev/null

if [[ -s "$stateDirectory/errors.log" ]]; then
    fail "$(tr '\n' ';' < "$stateDirectory/errors.log")"
fi
for route in \
    'GET /posts/1' \
    'GET /posts' \
    'POST /posts' \
    'GET /status/500' \
    'GET /invalid-json' \
    'GET /slow' \
    'GET /auth' \
    'POST /token' \
    'POST /multipart'; do
    if ! grep -Fqx "$route" "$stateDirectory/routes.log"; then
        fail "Missing request route: $route"
    fi
done
if [[ "$(grep -Fxc 'GET /status/500' "$stateDirectory/routes.log")" -ne 2 ]]; then
    fail 'Retry policy did not perform two status attempts.'
fi

printf '%s\n' 'Crossa local network integration tests passed'
