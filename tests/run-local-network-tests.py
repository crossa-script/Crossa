#!/usr/bin/env python3

import json
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


POST = {
    "userId": 1,
    "id": 1,
    "title": "local post",
    "body": "local response",
}


class MockRequestHandler(BaseHTTPRequestHandler):
    server_version = "CrossaMock/1.0"

    def do_GET(self):
        parsed = urlparse(self.path)
        self.server.requests.append(("GET", parsed.path, parsed.query))
        self._validate_headers(parsed.path)
        if parsed.path == "/posts/1":
            self._write_json(200, POST)
            return
        if parsed.path == "/posts":
            self._write_json(200, [POST])
            return
        if parsed.path == "/status/500":
            self._write_json(500, {"error": "server"})
            return
        if parsed.path == "/invalid-json":
            self._write_body(200, "{")
            return
        if parsed.path == "/slow":
            time.sleep(0.3)
            self._write_json(200, {"slow": True})
            return
        if parsed.path == "/auth":
            if self.headers.get("Authorization") == "Bearer fresh-token":
                self._write_json(200, {"authenticated": True})
            else:
                self._write_json(401, {"error": "expired"})
            return
        self._write_json(404, {"error": "not found"})

    def do_POST(self):
        parsed = urlparse(self.path)
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8")
        self.server.requests.append(("POST", parsed.path, body))
        self._validate_headers(parsed.path)
        if parsed.path == "/token":
            if "refresh_token=refresh-token" not in body:
                self.server.errors.append("Refresh token grant mismatch")
            self._write_json(200, {"access_token": "fresh-token"})
            return
        if parsed.path == "/multipart":
            if "multipart/form-data" not in self.headers.get("Content-Type", ""):
                self.server.errors.append("Multipart content type missing")
            if "name=\"description\"" not in body or "native" not in body:
                self.server.errors.append("Multipart description missing")
            if "name=\"payload\"" not in body or "crossa" not in body:
                self.server.errors.append("Multipart payload missing")
            self._write_json(200, {"uploaded": True})
            return
        if parsed.path == "/posts":
            try:
                if json.loads(body) != {
                    "title": "local",
                    "body": "mock",
                    "userId": 1,
                }:
                    self.server.errors.append("POST body mismatch")
            except json.JSONDecodeError:
                self.server.errors.append("POST body was not JSON")
            self._write_json(201, json.loads(body))
            return
        self._write_json(404, {"error": "not found"})

    def log_message(self, _format, *_args):
        return

    def _validate_headers(self, path):
        if path != "/posts/1":
            return
        expected = {
            "X-Crossa-Common": "enabled",
            "X-Crossa-Request": "get",
            "X-Crossa-Custom": "enabled",
        }
        for name, value in expected.items():
            if self.headers.get(name) != value:
                self.server.errors.append(
                    f"Missing header {name}: {self.headers.get(name)}"
                )
        if parse_qs(urlparse(self.path).query) != {"page": ["1"]}:
            self.server.errors.append("GET query parameters mismatch")

    def _write_json(self, status, value):
        self._write_body(status, json.dumps(value, separators=(",", ":")))

    def _write_body(self, status, body):
        encoded = body.encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(encoded)))
            self.end_headers()
            self.wfile.write(encoded)
        except (BrokenPipeError, ConnectionResetError):
            return


def run_case(
    binary, fixture, expected_status=0, expected_text=None, debug=False
):
    arguments = [binary, "test", str(fixture)]
    if debug:
        arguments.append("--debug")
    result = subprocess.run(
        arguments,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    if result.returncode != expected_status:
        raise RuntimeError(
            f"{fixture.name} returned {result.returncode}: {output}"
        )
    if expected_text is not None and expected_text not in output:
        raise RuntimeError(
            f"{fixture.name} did not contain '{expected_text}': {output}"
        )
    return output


def main():
    if len(sys.argv) != 2:
        raise RuntimeError("Usage: run-local-network-tests.py <crossa>")
    root = Path(__file__).resolve().parent.parent
    server = ThreadingHTTPServer(("127.0.0.1", 18765), MockRequestHandler)
    server.daemon_threads = True
    server.requests = []
    server.errors = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        local_output = run_case(
            sys.argv[1], root / "tests/local-network/request.cra", debug=True
        )
        network_logs = "\n".join(
            line for line in local_output.splitlines()
            if "Network request" in line
        )
        if "headerValues=" not in network_logs:
            raise RuntimeError("Enabled header logging did not appear.")
        if "body=" not in network_logs:
            raise RuntimeError("Enabled body logging did not appear.")
        if "secret-token" in network_logs:
            raise RuntimeError("Excluded Authorization header appeared in logs.")
        if "telemetry event=completed" not in local_output:
            raise RuntimeError("Structured telemetry event did not appear.")
        if "streamed=true" not in local_output or "downloadChunks=" not in local_output:
            raise RuntimeError("Transfer streaming metrics did not appear.")
        run_case(
            sys.argv[1],
            root / "tests/local-network/http-error.cra",
            1,
            "http/http_status",
        )
        run_case(
            sys.argv[1],
            root / "tests/local-network/invalid-json.cra",
            1,
            "serialization/invalid_json",
        )
        run_case(
            sys.argv[1],
            root / "tests/local-network/timeout.cra",
            1,
            "timeout",
        )
        if server.errors:
            raise RuntimeError("; ".join(server.errors))
        observed_routes = {
            (request[0], request[1]) for request in server.requests
        }
        required_routes = {
            ("GET", "/posts/1"),
            ("GET", "/posts"),
            ("POST", "/posts"),
            ("GET", "/status/500"),
            ("GET", "/invalid-json"),
            ("GET", "/slow"),
            ("GET", "/auth"),
            ("POST", "/token"),
            ("POST", "/multipart"),
        }
        if not required_routes.issubset(observed_routes):
            raise RuntimeError(
                f"Missing request routes: {required_routes - observed_routes}"
            )
        if sum(
            1 for request in server.requests
            if request[0] == "GET" and request[1] == "/status/500"
        ) != 2:
            raise RuntimeError("Retry policy did not perform two status attempts.")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    print("Crossa local network integration tests passed")


if __name__ == "__main__":
    main()
