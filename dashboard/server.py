#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = ["pyserial"]
# ///
"""Ground-station dashboard: read packets from the receiver, serve them live.

    python3 dashboard/server.py                      # live, auto-detects the board
    python3 dashboard/server.py --serial /dev/ttyACM0
    python3 dashboard/server.py --replay logs/some.log --speed 5

Then open http://localhost:8000. See dashboard/README.md.
"""

import argparse
import collections
import glob
import json
import pathlib
import queue
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import telemetry

ROOT = pathlib.Path(__file__).resolve().parent
STATIC = {
    "/": ("index.html", "text/html; charset=utf-8"),
    "/app.js": ("app.js", "text/javascript; charset=utf-8"),
    "/style.css": ("style.css", "text/css; charset=utf-8"),
}
HISTORY_PACKETS = 20000  # what a freshly opened page gets; the log keeps everything
SUBSCRIBER_BACKLOG = 2000
SSE_HEARTBEAT_SECONDS = 10
ESPRESSIF_USB_VID = 0x303a
RECONNECT_SECONDS = 1.0
MAX_REPLAY_GAP_SECONDS = 2.0


class Hub:
    """Fan decoded packets out to every open page, keeping recent history."""

    def __init__(self):
        self._lock = threading.Lock()
        self._history = collections.deque(maxlen=HISTORY_PACKETS)
        self._subscribers = set()
        self._clock = telemetry.TickClock()
        self._seq = 0
        self._status = {"state": "starting", "detail": "", "source": "serial"}

    def add_packet(self, raw, rssi, crc_ok, lqi):
        received_s = time.time()
        packet = telemetry.decode(raw)
        with self._lock:
            self._seq += 1
            packet.update(
                seq=self._seq,
                rx=int(received_s * 1000),
                t=self._clock.seconds(packet["serial"], packet["tick"], received_s),
                rssi=rssi,
                crc_ok=crc_ok,
                lqi=lqi,
                hex=raw.hex(),
            )
            self._history.append(packet)
            self._broadcast("packet", packet)

    def set_status(self, state, detail="", **extra):
        with self._lock:
            self._status = {**self._status, "state": state, "detail": detail, **extra}
            self._broadcast("status", self._status)

    def subscribe(self):
        """Register a page. Returns its queue and the state to start it from."""
        subscriber = queue.Queue()
        with self._lock:
            self._subscribers.add(subscriber)
            snapshot = {"status": self._status, "packets": list(self._history)}
        return subscriber, snapshot

    def unsubscribe(self, subscriber):
        with self._lock:
            self._subscribers.discard(subscriber)

    def _broadcast(self, event, data):
        for subscriber in list(self._subscribers):
            if subscriber.qsize() < SUBSCRIBER_BACKLOG:
                subscriber.put((event, data))
            else:
                # Too far behind to catch up: end the stream. The page's
                # EventSource reconnects and gets a fresh snapshot.
                self._subscribers.discard(subscriber)
                subscriber.put(None)


def find_serial_port():
    """Prefer the Espressif USB device, else any USB CDC or serial adapter."""
    from serial.tools import list_ports
    espressif = [p.device for p in list_ports.comports() if p.vid == ESPRESSIF_USB_VID]
    candidates = sorted(espressif) or sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    return candidates[0] if candidates else None


def open_log(log_dir):
    log_dir.mkdir(parents=True, exist_ok=True)
    path = log_dir / time.strftime("dashboard-%y%m%d-%H%M%S.log")
    return path, open(path, "a", encoding="utf-8", buffering=1)


def read_serial(hub, args):
    """Read the board until the process exits, riding out unplugs and resets."""
    import serial  # imported here so replay works without pyserial installed

    log_file = None
    parser = telemetry.LineParser()
    hub.set_status("waiting", "Looking for the receiver")
    while True:
        port = args.serial or find_serial_port()
        if port is None:
            hub.set_status("waiting", "No receiver found. Plug in the board.")
            time.sleep(RECONNECT_SECONDS)
            continue
        try:
            # exclusive makes a port held by `pio device monitor` fail clearly
            # instead of the two readers silently splitting the data.
            with serial.Serial(port, args.baud, timeout=0.2, exclusive=True) as device:
                if log_file is None:
                    log_path, log_file = open_log(args.log_dir)
                    hub.set_status("connected", port, log=str(log_path))
                else:
                    hub.set_status("connected", port)
                pending = b""
                while True:
                    pending += device.read(device.in_waiting or 1)
                    *lines, pending = pending.split(b"\n")
                    for line in lines:
                        text = line.decode("utf-8", "replace").rstrip("\r")
                        log_file.write(text + "\n")
                        if not args.quiet:
                            print(text, flush=True)
                        finished = parser.feed(text)
                        if finished:
                            hub.add_packet(*finished)
        except (OSError, serial.SerialException) as error:
            hub.set_status("waiting", f"{port}: {error}")
            time.sleep(RECONNECT_SECONDS)


def read_replay(hub, args):
    """Feed a captured log through at the pace its ticks say it arrived."""
    hub.set_status("replaying", str(args.replay), source="replay")
    parser = telemetry.LineParser()
    latest_tick = None
    with open(args.replay, encoding="utf-8", errors="replace") as log:
        for line in log:
            finished = parser.feed(line)
            if not finished:
                continue
            tick = int.from_bytes(finished[0][2:4], "little")
            if latest_tick is not None and args.speed > 0:
                delta = ((tick - latest_tick + 0x8000) & 0xffff) - 0x8000
                if delta > 0:
                    seconds = delta / telemetry.TICKS_PER_SECOND / args.speed
                    time.sleep(min(seconds, MAX_REPLAY_GAP_SECONDS))
            if latest_tick is None or ((tick - latest_tick) & 0xffff) < 0x8000:
                latest_tick = tick
            hub.add_packet(*finished)
    hub.set_status("finished", str(args.replay))


class Handler(BaseHTTPRequestHandler):
    hub = None
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/events":
            self._stream_events()
        elif path == "/schema":
            self._send(json.dumps(telemetry.schema()).encode(), "application/json")
        elif path in STATIC:
            name, content_type = STATIC[path]
            self._send((ROOT / name).read_bytes(), content_type)
        else:
            self.send_error(HTTPStatus.NOT_FOUND)

    def _send(self, body, content_type):
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _stream_events(self):
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        subscriber, snapshot = self.hub.subscribe()
        try:
            self._write_event("snapshot", snapshot)
            while True:
                try:
                    message = subscriber.get(timeout=SSE_HEARTBEAT_SECONDS)
                except queue.Empty:
                    self.wfile.write(b": keepalive\n\n")
                    self.wfile.flush()
                    continue
                if message is None:
                    return
                self._write_event(*message)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.hub.unsubscribe(subscriber)

    def _write_event(self, event, data):
        body = json.dumps(data, separators=(",", ":"))
        self.wfile.write(f"event: {event}\ndata: {body}\n\n".encode())
        self.wfile.flush()

    def log_message(self, *_):
        pass


def main():
    parser = argparse.ArgumentParser(description="Live telemetry dashboard for TeleDecode.")
    parser.add_argument("--serial", help="serial port (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--quiet", action="store_true",
                        help="don't print the board's serial output to this terminal")
    parser.add_argument("--replay", type=pathlib.Path, help="replay a captured log instead")
    parser.add_argument("--speed", type=float, default=1.0,
                        help="replay speed multiplier; 0 loads the whole log at once")
    parser.add_argument("--host", default="127.0.0.1",
                        help="address to serve on; 0.0.0.0 shares it on the network")
    parser.add_argument("--port", type=int, default=8000, help="web server port")
    parser.add_argument("--log-dir", type=pathlib.Path, default=ROOT.parent / "logs",
                        help="where live sessions are logged")
    args = parser.parse_args()

    hub = Hub()
    source = read_replay if args.replay else read_serial
    threading.Thread(target=source, args=(hub, args), daemon=True).start()

    Handler.hub = hub
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.daemon_threads = True
    print(f"Dashboard: http://{'localhost' if args.host == '127.0.0.1' else args.host}:{args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
