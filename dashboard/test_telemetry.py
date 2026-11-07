"""Decoder tests against packets captured from a TeleMega (serial 10978).

Run with: python3 -m unittest discover dashboard
"""

import math
import unittest

from telemetry import LineParser, TickClock, decode, schema, TYPES

MEGA_NORM = "e2 2a 03 32 14 4c fd ff 4d 85 01 00 4d 0a 99 00 1c fe f5 fa fe ff f2 ff 00 00 94 05 75 01 40 f3"
MEGA_DATA = "e2 2a 06 32 09 02 4e 0c 00 00 00 00 00 00 00 00 43 85 01 00 f2 ff e5 ff 0c 00 64 ff 8a fe e3 ff"
LOCATION_NO_FIX = "e2 2a a3 31 05 20 ef ff 00 00 00 00 00 00 00 00 0f 0a 12 00 02 07 e7 e7 e7 01 00 00 00 00 00 ff"
SATELLITES_NONE = "e2 2a a3 31 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ff"
CONFIGURATION = "e2 2a c3 02 04 23 04 00 01 1a 02 00 62 02 00 04 4b 45 32 43 4e 51 00 00 31 2e 39 2e 31 35 00 00"
NOISE = "cc 83 52 5a 1f c5 f2 e1 61 d1 a1 3c 09 bb ea ca 4d a9 be fb f1 68 7e 4a 4f 66 05 36 a6 59 48 36"


def packet(hex_text):
    return decode(bytes.fromhex(hex_text))


class DecodeTest(unittest.TestCase):
    def test_header_matches_firmware_console_output(self):
        p = packet(MEGA_NORM)
        self.assertEqual((p["serial"], p["tick"], p["type"]), (10978, 12803, 20))

    def test_mega_norm_is_physically_plausible_on_the_pad(self):
        f = packet(MEGA_NORM)["f"]
        self.assertAlmostEqual(f["pressure"], 99661)
        self.assertAlmostEqual(f["temperature"], 26.37)
        gravity = math.sqrt(f["accel_along"] ** 2 + f["accel_across"] ** 2
                            + f["accel_through"] ** 2)
        self.assertAlmostEqual(gravity, 9.81, delta=0.2)

    def test_orient_above_127_degrees_stays_positive(self):
        raw = bytearray.fromhex(MEGA_NORM)
        raw[5] = 135
        self.assertEqual(decode(bytes(raw))["f"]["orient"], 135)

    def test_mega_data_battery_uses_altos_divider(self):
        f = packet(MEGA_DATA)["f"]
        self.assertAlmostEqual(f["battery"], 3.3 * 3150 / 4095 * 1.56, places=3)
        self.assertEqual(f["state"], 2)
        self.assertEqual(f["height"], -29)
        self.assertAlmostEqual(f["speed"], -374 / 16)
        self.assertAlmostEqual(f["acceleration"], -156 / 16)

    def test_sense_expands_to_12_bits(self):
        raw = bytearray.fromhex(MEGA_DATA)
        raw[14] = 0xff
        f = decode(bytes(raw))["f"]
        self.assertAlmostEqual(f["apogee_v"], 3.3 * 127 / 27, places=3)

    def test_30v_type_uses_its_own_divider(self):
        raw = bytearray.fromhex(MEGA_DATA)
        raw[4] = 0x15
        raw[8:10] = (4095).to_bytes(2, "little")
        self.assertAlmostEqual(decode(bytes(raw))["f"]["v_pyro"], 3.3 * 112 / 12, places=3)

    def test_location_without_lock_hides_position(self):
        f = packet(LOCATION_NO_FIX)["f"]
        self.assertFalse(f["locked"])
        self.assertTrue(f["running"])
        self.assertEqual(f["nsat"], 0)
        self.assertIsNone(f["latitude"])
        self.assertIsNone(f["altitude"])

    def test_location_with_lock(self):
        raw = bytearray.fromhex(LOCATION_NO_FIX)
        raw[5] = 0x10 | 0x20 | 8
        raw[8:12] = int(40.1164 * 1e7).to_bytes(4, "little", signed=True)
        raw[12:16] = int(-88.2434 * 1e7).to_bytes(4, "little", signed=True)
        raw[6:8] = (0x1234).to_bytes(2, "little")
        raw[31] = 0x01
        f = decode(bytes(raw))["f"]
        self.assertEqual(f["nsat"], 8)
        self.assertAlmostEqual(f["latitude"], 40.1164, places=4)
        self.assertAlmostEqual(f["longitude"], -88.2434, places=4)
        self.assertEqual(f["altitude"], 0x11234)

    def test_negative_24_bit_altitude_sign_extends(self):
        raw = bytearray.fromhex(LOCATION_NO_FIX)
        raw[5] = 0x10 | 0x20
        self.assertEqual(decode(bytes(raw))["f"]["altitude"], -17)

    def test_satellites(self):
        self.assertEqual(packet(SATELLITES_NONE)["f"]["sats"], [])
        raw = bytearray.fromhex(SATELLITES_NONE)
        raw[5:10] = bytes([2, 12, 40, 7, 33])
        self.assertEqual(decode(bytes(raw))["f"]["sats"],
                         [{"svid": 12, "cn0": 40}, {"svid": 7, "cn0": 33}])

    def test_configuration(self):
        f = packet(CONFIGURATION)["f"]
        self.assertEqual(f["device_name"], "TeleMega")
        self.assertEqual(f["callsign"], "KE2CNQ")
        self.assertEqual(f["version"], "1.9.15")
        self.assertEqual(f["flight"], 4)
        self.assertEqual(f["apogee_delay"], 2)
        self.assertEqual(f["main_deploy"], 610)

    def test_unknown_type_keeps_header_only(self):
        p = packet(NOISE)
        self.assertFalse(p["known"])
        self.assertEqual(p["type"], 31)
        self.assertEqual(p["f"], {})

    def test_wrong_length_is_rejected(self):
        with self.assertRaises(ValueError):
            decode(b"\x00" * 31)

    def test_every_layout_fits_the_packet_and_decodes(self):
        for type_id, packet_type in TYPES.items():
            for f in packet_type.fields:
                self.assertLessEqual(f.offset + f.size, 32, (type_id, f.name))
            raw = bytearray(32)
            raw[4] = type_id
            self.assertTrue(decode(bytes(raw))["known"])
            names = {f.name for f in packet_type.fields}
            self.assertTrue(set(packet_type.summary) - names <= {"device_name", "nsat", "locked"},
                            type_id)
        self.assertIn("20", schema()["types"])


class TickClockTest(unittest.TestCase):
    def test_counts_past_the_wrap(self):
        clock = TickClock()
        self.assertEqual(clock.seconds(1, 65000, 0), 650.0)
        self.assertAlmostEqual(clock.seconds(1, 400, 9.36), 659.36)

    def test_older_tick_does_not_look_like_a_wrap(self):
        clock = TickClock()
        clock.seconds(1, 1000, 0)
        self.assertEqual(clock.seconds(1, 900, 0), 9.0)
        self.assertEqual(clock.seconds(1, 1100, 1), 11.0)

    def test_dropout_longer_than_a_wrap(self):
        clock = TickClock()
        clock.seconds(1, 1000, 0)
        self.assertAlmostEqual(clock.seconds(1, 2000, 665.36), 665.36 + 10 - 0.0, delta=0.01)

    def test_devices_are_independent(self):
        clock = TickClock()
        clock.seconds(1, 60000, 0)
        self.assertEqual(clock.seconds(2, 10, 0), 0.1)


class LineParserTest(unittest.TestCase):
    def feed(self, *lines):
        parser = LineParser()
        return [out for out in map(parser.feed, lines) if out]

    def test_parses_firmware_block(self):
        out = self.feed("packet rx success", MEGA_DATA + " ", "serial: 10978", "tick: 12806",
                        "type: 9", "rssi: -43.000000", "battery: 3150", "-------------------")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), -43.0, None, None)])

    def test_current_format_with_status_bytes(self):
        hex_line = MEGA_DATA + " e1 85 00 00 33 33 " + "00 " * 26
        out = self.feed("CRC_OK: 1, Link Quality (Lower is Better): 66", "rssi: -30.000000",
                        "packet: " + hex_line, "serial: 10978", "tick: 12806", "type: 9",
                        "-------------------")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), -30.0, True, 5)])

    def test_status_byte_overrides_printed_crc_flag(self):
        out = self.feed("CRC_OK: 0, Link Quality (Lower is Better): 67", "rssi: -30.000000",
                        MEGA_DATA + " e1 86 00 00", "-------------------")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), -30.0, True, 6)])

    def test_crc_failure_comes_from_bit_7(self):
        out = self.feed("CRC_OK: 1, Link Quality (Lower is Better): 2", "rssi: -95.000000",
                        NOISE + " a0 05 00 00", "-------------------")
        self.assertEqual(out, [(bytes.fromhex(NOISE), -95.0, False, 5)])

    def test_hex_on_the_crc_line_when_the_newline_is_missing(self):
        out = self.feed("CRC_OK: 0, Link Quality (Lower is Better): 73" + MEGA_DATA + " e1 85 00 00",
                        "serial: 10978", "rssi: -26.000000", "-------------------")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), -26.0, True, 5)])

    def test_new_block_finishes_one_missing_its_terminator(self):
        out = self.feed("rssi: -30.000000", MEGA_DATA + " e1 85 00 00",
                        "CRC_OK: 1, Link Quality (Lower is Better): 66")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), -30.0, True, 5)])

    def test_rssi_does_not_leak_into_the_next_block(self):
        out = self.feed("rssi: -30.000000", "-------------------", MEGA_DATA + " e1 85 00 00", "-----")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), None, True, 5)])

    def test_log_from_before_rssi(self):
        out = self.feed("packet rx success", MEGA_DATA, "type: 9", "-------------------")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), None, None, None)])

    def test_invalid_rssi_becomes_none(self):
        out = self.feed(MEGA_DATA, "-128 dBm received; invalid RSSI", "rssi: -128.000000", "-----")
        self.assertEqual(out, [(bytes.fromhex(MEGA_DATA), None, None, None)])

    def test_missing_terminator_still_emits(self):
        out = self.feed("packet rx success", MEGA_DATA, "packet rx success", MEGA_NORM, "-----")
        self.assertEqual([block[0] for block in out],
                         [bytes.fromhex(MEGA_DATA), bytes.fromhex(MEGA_NORM)])

    def test_truncated_hex_and_console_noise_are_ignored(self):
        out = self.feed("Boot", "Unknown command; send HELP", "packet rx success",
                        "e2 2a 03 32 14", "-------------------")
        self.assertEqual(out, [])


if __name__ == "__main__":
    unittest.main()
