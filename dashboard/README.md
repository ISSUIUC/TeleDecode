# Telemetry dashboard

A local web dashboard for everything the receiver hears. `server.py` reads the
firmware's serial output, decodes each packet, and streams it to the page.

Nothing loads from the internet, so it works at a launch site. That also means
the GPS track is drawn on a plain grid, with no map behind it.

## Run it

Close `pio device monitor` first, since only one program can hold the port. You
don't need it alongside the dashboard: the server prints the board's serial
output to its terminal, the same text the monitor shows. Add `--quiet` to turn
that off.

```sh
python3 dashboard/server.py          # needs pyserial
uv run dashboard/server.py           # or let uv install it
```

Open <http://localhost:8000>. The server finds the board by itself and
reconnects if it's unplugged or reset. Use `--serial /dev/ttyACM0` to choose a
port, and `--host 0.0.0.0` to let others on your network view it.

Replay a captured log, with no hardware needed:

```sh
python3 dashboard/server.py --replay logs/some.log --speed 5
python3 dashboard/server.py --replay logs/some.log --speed 0   # load it all at once
```

## Logging

Live sessions are saved to `logs/dashboard-<date>-<time>.log` in the same format
as `pio device monitor` logs, so they can be replayed. The page only keeps the
newest 20,000 packets; the log keeps everything.

## Reading the data

- **Decoding** uses the hex dump line. The firmware's own `serial:` and `type:`
  lines are ignored, so only `rssi:` and the hex format matter to the parser.
  Older logs without status bytes still load.
- **CRC and link quality** come from the radio's status byte after the packet
  (byte 33: bit 7 is CRC OK, bits 6:0 are LQI), not the printed `CRC_OK:` line.
- **Conversions** follow AltOS's `altoslib`, not the comments in
  `src/ao_telemetry.h`. The differences are listed at the top of `telemetry.py`.
- **Suspect packets** failed CRC, have an unknown type, or have a serial seen
  fewer than 3 times. They appear in the packet list but stay out of the charts.
- **Time** on the charts is board uptime in `m:ss`.

## Tests

```sh
python3 -m unittest discover dashboard
```
