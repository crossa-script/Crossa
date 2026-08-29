#!/usr/bin/env bash

stateDirectory="${CROSSA_LOCAL_NETWORK_STATE_DIRECTORY:?}"
method=''
target=''
protocol=''
commonHeader=''
requestHeader=''
customHeader=''
authorizationHeader=''
contentType=''
contentLength='0'

addError() {
    printf '%s\n' "$1" >> "$stateDirectory/errors.log"
}

writeResponse() {
    local status="$1"
    local body="$2"
    local reason='OK'

    case "$status" in
        201)
            reason='Created'
            ;;
        401)
            reason='Unauthorized'
            ;;
        404)
            reason='Not Found'
            ;;
        500)
            reason='Internal Server Error'
            ;;
    esac

    printf 'HTTP/1.1 %s %s\r\n' "$status" "$reason"
    printf 'Content-Type: application/json\r\n'
    printf 'Content-Length: %s\r\n' "${#body}"
    printf 'Connection: close\r\n\r\n'
    printf '%s' "$body"
}

IFS=' ' read -r method target protocol
while IFS= read -r headerLine; do
    headerLine="${headerLine%$'\r'}"
    if [[ -z "$headerLine" ]]; then
        break
    fi
    case "$headerLine" in
        'X-Crossa-Common: '*)
            commonHeader="${headerLine#*: }"
            ;;
        'X-Crossa-Request: '*)
            requestHeader="${headerLine#*: }"
            ;;
        'X-Crossa-Custom: '*)
            customHeader="${headerLine#*: }"
            ;;
        'Authorization: '*)
            authorizationHeader="${headerLine#*: }"
            ;;
        'Content-Type: '*)
            contentType="${headerLine#*: }"
            ;;
        'Content-Length: '*)
            contentLength="${headerLine#*: }"
            ;;
    esac
done
body=''
if [[ "$contentLength" -gt 0 ]]; then
    body="$(dd bs=1 count="$contentLength" 2>/dev/null)"
fi
target="${target%$'\r'}"
path="${target%%\?*}"
query=''
if [[ "$target" == *'?'* ]]; then
    query="${target#*\?}"
fi
printf '%s %s\n' "$method" "$path" >> "$stateDirectory/routes.log"

if [[ "$path" == '/posts/1' ]]; then
    if [[ "$commonHeader" != 'enabled' ]]; then
        addError 'Missing X-Crossa-Common header.'
    fi
    if [[ "$requestHeader" != 'get' ]]; then
        addError 'Missing X-Crossa-Request header.'
    fi
    if [[ "$customHeader" != 'enabled' ]]; then
        addError 'Missing X-Crossa-Custom header.'
    fi
    if [[ "$query" != 'page=1' ]]; then
        addError 'GET query parameters mismatch.'
    fi
    writeResponse 200 '{"userId":1,"id":1,"title":"local post","body":"local response"}'
    exit 0
fi

if [[ "$method" == 'GET' && "$path" == '/posts' ]]; then
    writeResponse 200 '[{"userId":1,"id":1,"title":"local post","body":"local response"}]'
    exit 0
fi

if [[ "$method" == 'POST' && "$path" == '/posts' ]]; then
    if [[ "$body" != '{"title":"local","body":"mock","userId":1}' ]]; then
        addError 'POST body mismatch.'
    fi
    writeResponse 201 "$body"
    exit 0
fi

if [[ "$method" == 'GET' && "$path" == '/status/500' ]]; then
    writeResponse 500 '{"error":"server"}'
    exit 0
fi

if [[ "$method" == 'GET' && "$path" == '/invalid-json' ]]; then
    writeResponse 200 '{'
    exit 0
fi

if [[ "$method" == 'GET' && "$path" == '/slow' ]]; then
    sleep 0.3
    writeResponse 200 '{"slow":true}'
    exit 0
fi

if [[ "$method" == 'GET' && "$path" == '/auth' ]]; then
    if [[ "$authorizationHeader" == 'Bearer fresh-token' ]]; then
        writeResponse 200 '{"authenticated":true}'
    else
        writeResponse 401 '{"error":"expired"}'
    fi
    exit 0
fi

if [[ "$method" == 'POST' && "$path" == '/token' ]]; then
    if [[ "$body" != *'refresh_token=refresh-token'* ]]; then
        addError 'Refresh token grant mismatch.'
    fi
    writeResponse 200 '{"access_token":"fresh-token"}'
    exit 0
fi

if [[ "$method" == 'POST' && "$path" == '/multipart' ]]; then
    if [[ "$contentType" != *'multipart/form-data'* ]]; then
        addError 'Multipart content type missing.'
    fi
    if [[ "$body" != *'name="description"'* || "$body" != *'native'* ]]; then
        addError 'Multipart description missing.'
    fi
    if [[ "$body" != *'name="payload"'* || "$body" != *'crossa'* ]]; then
        addError 'Multipart payload missing.'
    fi
    writeResponse 200 '{"uploaded":true}'
    exit 0
fi

writeResponse 404 '{"error":"not found"}'
