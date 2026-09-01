#!/usr/bin/env python3
"""Mockserver til klimasensoren — test hele upload-flowet uden den rigtige server.

Brug:
    python3 tools/mockserver.py [port]          # default 8080

Modtager POST /api/v1/measurements, printer målingerne og kvitterer alt.
- Læg en 'mock_config.json' ved siden af scriptet for at sende config ned,
  fx: {"measureIntervalS": 600, "uploadIntervalS": 1800}
- Læg en 'mock_ota.json' for at udstede en OTA-opdatering,
  fx: {"version": "1.1.0", "url": "http://<ip>:8080/fw.bin", "sha256": "<hex>"}
  (GET /fw.bin serverer filen 'fw.bin' fra samme mappe.)
"""
import http.server
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).parent
EXPECTED_TOKEN = "123456789"


def load_optional(name):
    p = HERE / name
    if p.exists():
        try:
            return json.loads(p.read_text())
        except json.JSONDecodeError as e:
            print(f"!! {name} er ikke gyldig JSON: {e}")
    return None


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/api/v1/measurements":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        try:
            body = json.loads(self.rfile.read(length))
        except json.JSONDecodeError:
            self.send_error(400, "ugyldig JSON")
            return

        if body.get("token") != EXPECTED_TOKEN:
            print(f"!! forkert token: {body.get('token')!r}")
            self.send_error(401)
            return

        measurements = body.get("measurements", [])
        print(f"\n== {body.get('deviceId')} fw={body.get('fwVersion')} "
              f"storageFull={body.get('storageFull')} ({len(measurements)} målinger)")
        for m in measurements:
            ts = m.get("ts")
            when = (datetime.fromtimestamp(ts, tz=timezone.utc).isoformat()
                    if m.get("tsValid", True) and ts and ts > 10**9 else f"rel:{ts}")
            acc = m.get("accel") or {}
            print(f"  {when}  T={m.get('tempC')}°C RH={m.get('rh')}% "
                  f"PM2.5={m.get('pm2_5')} PM10={m.get('pm10')} lux={m.get('lux')} "
                  f"acc=({acc.get('x')},{acc.get('y')},{acc.get('z')})")

        response = {"ackCount": len(measurements)}
        cfg = load_optional("mock_config.json")
        if cfg:
            response["config"] = cfg
        ota = load_optional("mock_ota.json")
        if ota:
            response["ota"] = ota

        data = json.dumps(response).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/fw.bin" and (HERE / "fw.bin").exists():
            data = (HERE / "fw.bin").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            self.send_error(404)

    def log_message(self, *args):
        pass  # egen logning ovenfor


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    print(f"Mockserver på http://0.0.0.0:{port}/api/v1/measurements (token {EXPECTED_TOKEN})")
    http.server.HTTPServer(("", port), Handler).serve_forever()
