# TeleDecode

The receiver boots in its existing pad-mode telemetry configuration. Auto/idle
mode uses the Altos Packet Link protocol and is enabled explicitly from the
serial console at 9600 baud, with newline line endings:

- `CALL <callsign>` sets and saves the receiver callsign (1-8 characters).
- `IDLE` starts Packet Link using the saved callsign.
- `PAD` stops Packet Link and restores the radio settings saved on entry.
- `HELP` prints the commands.

Packet Link is bidirectional: the receiver sends sync packets and the
`E 0` monitor command. Match the transmitter's callsign and radio frequency.
The callsign defaults to `N0CALL` if no saved value is available.
