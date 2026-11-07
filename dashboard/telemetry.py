"""Decode AltOS telemetry from the TeleDecode firmware's serial output.

Layouts mirror src/ao_telemetry.h. Unit conversions come from AltOS's altoslib
(AltosConvert, AltosIMU, AltosMag and the AltosTelemetry* classes), which is
what AltosUI itself uses. Where those disagree with the comments in
ao_telemetry.h, altoslib wins, because it matches captured data:

  * `pres` is plain Pa, not "Pa * 10" (a captured 99661 is 99.7 kPa at ground).
  * pdop/hdop/vdop are DOP * 10, not "m * 5".
  * sense[] is the top 8 bits of a 12-bit ADC reading.

One exception: altoslib reads `orient` as signed, but the firmware computes
0-180 degrees and sends it as uint8_t, so it is decoded as unsigned here.
"""

import re
import struct
from dataclasses import dataclass
from typing import Callable, Optional

PACKET_SIZE = 32
TICKS_PER_SECOND = 100
GRAVITY = 9.80665
IGNITER_GOOD_VOLTS = 3.5  # AltosLib.ao_igniter_good
INVALID_RSSI = -128  # CC1120::getRSSI() reports this when RSSI is not valid

FLIGHT_STATES = [
    "startup", "idle", "pad", "boost", "fast", "coast",
    "drogue", "main", "landed", "invalid", "stateless",
]

PRODUCTS = {
    0x0b: "TeleMetrum", 0x0c: "TeleDongle", 0x0d: "EasyTimer", 0x0e: "TeleBT",
    0x0f: "TeleLaunch", 0x10: "TeleLCO", 0x11: "TeleScience", 0x12: "TelePyro",
    0x23: "TeleMega", 0x24: "MegaDongle", 0x25: "TeleGPS", 0x26: "EasyMini",
    0x27: "TeleMini", 0x28: "EasyMega", 0x2c: "EasyMotor",
}
PRODUCT_TELEGPS = 0x25


# --- Unit conversions ------------------------------------------------------

def _adc12_battery(raw):
    return 3.3 * raw / 4095.0 * (5.6 + 10.0) / 10.0


def _adc12_pyro_15v(raw):
    return 3.3 * raw / 4095.0 * (100.0 + 27.0) / 27.0


def _adc12_pyro_30v(raw):
    return 3.3 * raw / 4095.0 * (100.0 + 12.0) / 12.0


def _sense_12bit(pyro):
    """Expand an 8-bit sense byte back to 12 bits, then convert to volts."""
    return lambda raw: pyro((raw << 4) | (raw >> 4))


def _mini2_volts(raw):
    return raw / 32767.0 * 3.3 * 127 / 27


def _cc_battery(raw):
    return raw / 32767.0 * 5.0


def _cc_igniter(raw):
    return raw / 32767 * 15.0


def _baro_to_pa(raw):
    return ((raw / 16.0) / 2047.0 + 0.095) / 0.009 * 1000.0


def _thermo_to_c(raw):
    return (raw - 19791.268) / 32728.0 * 1.25 / 0.00247


def _per16(raw):
    return raw / 16.0


def _centi(raw):
    return raw / 100.0


def _deci(raw):
    return raw / 10.0


def _degrees(raw):
    return raw * 1e-7


def _counts(per_unit, sign=1):
    return lambda raw: sign * raw / per_unit


def _accel_counts(counts_per_g, sign=1):
    return lambda raw: sign * raw / counts_per_g * GRAVITY


def _cstring(raw):
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")


# --- Packet layouts --------------------------------------------------------

@dataclass(frozen=True)
class F:
    name: str
    offset: int
    fmt: str
    label: str
    unit: str = ""
    conv: Optional[Callable] = None

    @property
    def size(self):
        return struct.calcsize("<" + self.fmt)


@dataclass(frozen=True)
class PacketType:
    name: str
    fields: tuple
    summary: tuple = ()
    post: Optional[Callable] = None


HEADER = (
    F("serial", 0, "H", "Serial"),
    F("tick", 2, "H", "Tick", "1/100 s"),
    F("type", 4, "B", "Type"),
)

KALMAN_HEIGHT = "Height (Kalman)"
KALMAN_SPEED = "Speed (Kalman)"
KALMAN_ACCEL = "Acceleration (Kalman)"
BOARD_SUMMARY = ("state", "height", "speed", "battery")


def _sensor(with_accel, with_sense):
    fields = [
        F("state", 5, "B", "Flight state"),
        F("pressure", 8, "h", "Pressure", "Pa", _baro_to_pa),
        F("temperature", 10, "h", "Temperature", "°C", _thermo_to_c),
        F("battery", 12, "h", "Battery", "V", _cc_battery),
        F("acceleration", 18, "h", KALMAN_ACCEL, "m/s²", _per16),
        F("speed", 20, "h", KALMAN_SPEED, "m/s", _per16),
        F("height", 22, "h", KALMAN_HEIGHT, "m"),
        F("ground_pressure", 24, "h", "Ground pressure", "Pa", _baro_to_pa),
    ]
    if with_accel:
        fields += [
            F("accel_raw", 6, "h", "Accelerometer", "counts"),
            F("ground_accel", 26, "h", "Ground accel", "counts"),
            F("accel_plus_g", 28, "h", "Accel +1 g cal", "counts"),
            F("accel_minus_g", 30, "h", "Accel −1 g cal", "counts"),
        ]
    if with_sense:
        fields += [
            F("apogee_v", 14, "h", "Apogee continuity", "V", _cc_igniter),
            F("main_v", 16, "h", "Main continuity", "V", _cc_igniter),
        ]
    return tuple(fields)


def _imu_common():
    return (
        F("orient", 5, "B", "Angle from vertical", "°"),
        F("accel_raw", 6, "h", "Z accelerometer", "counts"),
        F("pressure", 8, "i", "Pressure", "Pa"),
        F("temperature", 12, "h", "Temperature", "°C", _centi),
    )


def _mega_norm(counts_per_g, counts_per_dps, counts_per_gauss):
    return _imu_common() + (
        F("accel_along", 14, "h", "Accel along", "m/s²", _accel_counts(counts_per_g)),
        F("accel_across", 16, "h", "Accel across", "m/s²", _accel_counts(counts_per_g)),
        F("accel_through", 18, "h", "Accel through", "m/s²", _accel_counts(counts_per_g)),
        F("gyro_roll", 20, "h", "Gyro roll", "°/s", _counts(counts_per_dps)),
        F("gyro_pitch", 22, "h", "Gyro pitch", "°/s", _counts(counts_per_dps)),
        F("gyro_yaw", 24, "h", "Gyro yaw", "°/s", _counts(counts_per_dps)),
        F("mag_along", 26, "h", "Mag along", "G", _counts(counts_per_gauss)),
        F("mag_across", 28, "h", "Mag across", "G", _counts(counts_per_gauss)),
        F("mag_through", 30, "h", "Mag through", "G", _counts(counts_per_gauss)),
    )


# The MEGA_SENSOR axes are chip-relative, so board-relative names depend on the
# IMU's mounting (AltosTelemetryMegaSensor). Note mag z comes before mag y.
MPU_COUNTS_PER_G = 2048.0
MPU_COUNTS_PER_DPS = 32767.0 / 2000.0
MPU_COUNTS_PER_GAUSS = 32767.0 / 48.0
HMC5883_COUNTS_PER_GAUSS = 1090.0
TELEMEGA_V3_MIN_SERIAL = 3000  # AltOS: lower serials are TeleMega v1/v2

MEGA_SENSOR_MPU = _imu_common() + (
    F("accel_across", 14, "h", "Accel across", "m/s²", _accel_counts(MPU_COUNTS_PER_G)),
    F("accel_along", 16, "h", "Accel along", "m/s²", _accel_counts(MPU_COUNTS_PER_G)),
    F("accel_through", 18, "h", "Accel through", "m/s²", _accel_counts(MPU_COUNTS_PER_G)),
    F("gyro_pitch", 20, "h", "Gyro pitch", "°/s", _counts(MPU_COUNTS_PER_DPS)),
    F("gyro_roll", 22, "h", "Gyro roll", "°/s", _counts(MPU_COUNTS_PER_DPS)),
    F("gyro_yaw", 24, "h", "Gyro yaw", "°/s", _counts(MPU_COUNTS_PER_DPS)),
    F("mag_across", 26, "h", "Mag across", "G", _counts(MPU_COUNTS_PER_GAUSS)),
    F("mag_through", 28, "h", "Mag through", "G", _counts(MPU_COUNTS_PER_GAUSS)),
    F("mag_along", 30, "h", "Mag along", "G", _counts(MPU_COUNTS_PER_GAUSS)),
)

MEGA_SENSOR_BMX160 = _imu_common() + (
    F("accel_along", 14, "h", "Accel along", "m/s²", _accel_counts(2048.0)),
    F("accel_across", 16, "h", "Accel across", "m/s²", _accel_counts(2048.0, -1)),
    F("accel_through", 18, "h", "Accel through", "m/s²", _accel_counts(2048.0)),
    F("gyro_roll", 20, "h", "Gyro roll", "°/s", _counts(MPU_COUNTS_PER_DPS)),
    F("gyro_pitch", 22, "h", "Gyro pitch", "°/s", _counts(MPU_COUNTS_PER_DPS)),
    F("gyro_yaw", 24, "h", "Gyro yaw", "°/s", _counts(MPU_COUNTS_PER_DPS)),
    F("mag_along", 26, "h", "Mag along", "G", _counts(100.0)),
    F("mag_through", 28, "h", "Mag through", "G", _counts(100.0)),
    F("mag_across", 30, "h", "Mag across", "G", _counts(100.0, -1)),
)


def _mega_data(pyro):
    sense = _sense_12bit(pyro)
    return (
        F("state", 5, "B", "Flight state"),
        F("battery", 6, "h", "Battery", "V", _adc12_battery),
        F("v_pyro", 8, "h", "Pyro battery", "V", pyro),
        F("igniter_a", 10, "B", "Igniter A continuity", "V", sense),
        F("igniter_b", 11, "B", "Igniter B continuity", "V", sense),
        F("igniter_c", 12, "B", "Igniter C continuity", "V", sense),
        F("igniter_d", 13, "B", "Igniter D continuity", "V", sense),
        F("apogee_v", 14, "B", "Apogee continuity", "V", sense),
        F("main_v", 15, "B", "Main continuity", "V", sense),
        F("ground_pressure", 16, "i", "Ground pressure", "Pa"),
        F("ground_accel", 20, "h", "Ground accel", "counts"),
        F("accel_plus_g", 22, "h", "Accel +1 g cal", "counts"),
        F("accel_minus_g", 24, "h", "Accel −1 g cal", "counts"),
        F("acceleration", 26, "h", KALMAN_ACCEL, "m/s²", _per16),
        F("speed", 28, "h", KALMAN_SPEED, "m/s", _per16),
        F("height", 30, "h", KALMAN_HEIGHT, "m"),
    )


METRUM_SENSOR = (
    F("state", 5, "B", "Flight state"),
    F("accel_raw", 6, "h", "Z accelerometer", "counts"),
    F("pressure", 8, "i", "Pressure", "Pa"),
    F("temperature", 12, "h", "Temperature", "°C", _centi),
    F("acceleration", 14, "h", KALMAN_ACCEL, "m/s²", _per16),
    F("speed", 16, "h", KALMAN_SPEED, "m/s", _per16),
    F("height", 18, "h", KALMAN_HEIGHT, "m"),
    F("battery", 20, "h", "Battery", "V", _adc12_battery),
    F("apogee_v", 22, "h", "Apogee continuity", "V", _adc12_pyro_15v),
    F("main_v", 24, "h", "Main continuity", "V", _adc12_pyro_15v),
)

METRUM_DATA = (
    F("ground_pressure", 8, "i", "Ground pressure", "Pa"),
    F("ground_accel", 12, "h", "Ground accel", "counts"),
    F("accel_plus_g", 14, "h", "Accel +1 g cal", "counts"),
    F("accel_minus_g", 16, "h", "Accel −1 g cal", "counts"),
)


def _mini(battery, pyro):
    return (
        F("state", 5, "B", "Flight state"),
        F("battery", 6, "h", "Battery", "V", battery),
        F("apogee_v", 8, "h", "Apogee continuity", "V", pyro),
        F("main_v", 10, "h", "Main continuity", "V", pyro),
        F("pressure", 12, "i", "Pressure", "Pa"),
        F("temperature", 16, "h", "Temperature", "°C", _centi),
        F("acceleration", 18, "h", KALMAN_ACCEL, "m/s²", _per16),
        F("speed", 20, "h", KALMAN_SPEED, "m/s", _per16),
        F("height", 22, "h", KALMAN_HEIGHT, "m"),
        F("ground_pressure", 24, "i", "Ground pressure", "Pa"),
    )


CONFIGURATION = (
    F("device", 5, "B", "Device type"),
    F("flight", 6, "H", "Flight number"),
    F("config_major", 8, "B", "Config major"),
    F("config_minor", 9, "B", "Config minor"),
    F("apogee_delay", 10, "H", "Apogee delay", "s"),
    F("main_deploy", 12, "H", "Main deploy", "m"),
    F("flight_log_max", 14, "H", "Max flight log", "kB"),
    F("callsign", 16, "8s", "Callsign", "", _cstring),
    F("version", 24, "8s", "Firmware", "", _cstring),
)

LOCATION = (
    F("flags", 5, "B", "GPS flags"),
    F("altitude", 6, "H", "GPS altitude", "m"),
    F("latitude", 8, "i", "Latitude", "°", _degrees),
    F("longitude", 12, "i", "Longitude", "°", _degrees),
    F("year", 16, "B", "Year"),
    F("month", 17, "B", "Month"),
    F("day", 18, "B", "Day"),
    F("hour", 19, "B", "Hour"),
    F("minute", 20, "B", "Minute"),
    F("second", 21, "B", "Second"),
    F("pdop", 22, "B", "PDOP", "", _deci),
    F("hdop", 23, "B", "HDOP", "", _deci),
    F("vdop", 24, "B", "VDOP", "", _deci),
    F("mode", 25, "B", "GPS mode"),
    F("ground_speed", 26, "H", "Ground speed", "m/s", _centi),
    F("climb_rate", 28, "h", "Climb rate", "m/s", _centi),
    F("course", 30, "B", "Course", "°", lambda raw: raw * 2),
    F("altitude_high", 31, "b", "Altitude high byte"),
)

_GPS_ONLY_WHEN_LOCKED = (
    "altitude", "latitude", "longitude", "year", "month", "day", "hour",
    "minute", "second", "ground_speed", "climb_rate", "course",
)


def _post_location(f, raw, serial):
    flags = f["flags"]
    f["nsat"] = flags & 0x0f
    f["locked"] = bool(flags & 0x10)
    f["running"] = bool(flags & 0x20)
    f["date_valid"] = bool(flags & 0x40)
    f["course_valid"] = bool(flags & 0x80)
    if f["mode"] & 0x01:  # AO_GPS_MODE_ALTITUDE_24
        f["altitude"] = (f["altitude_high"] << 16) | f["altitude"]
    else:
        f["altitude"] = struct.unpack_from("<h", raw, 6)[0]
    if not f["locked"]:
        for name in _GPS_ONLY_WHEN_LOCKED:
            f[name] = None


def _post_satellite(f, raw, serial):
    count = min(f["channels"], 12)
    f["sats"] = [{"svid": raw[6 + i * 2], "cn0": raw[7 + i * 2]} for i in range(count)]


def _post_companion(f, raw, serial):
    count = min(f["channels"], 12)
    f["data"] = list(struct.unpack_from("<%dH" % count, raw, 8))


def _post_baro(f, raw, serial):
    f["baro"] = list(struct.unpack_from("<12h", raw, 6))


def _post_configuration(f, raw, serial):
    f["device_name"] = PRODUCTS.get(f["device"], "Device 0x%02x" % f["device"])
    if f["device"] == PRODUCT_TELEGPS:  # TeleGPS reuses this slot for v_batt
        f["apogee_delay"] = None


def _post_mega_sensor_mpu(f, raw, serial):
    if serial < TELEMEGA_V3_MIN_SERIAL:
        for axis in ("mag_along", "mag_across", "mag_through"):
            f[axis] = f[axis] * MPU_COUNTS_PER_GAUSS / HMC5883_COUNTS_PER_GAUSS


TYPES = {
    0x01: PacketType("TeleMetrum sensor", _sensor(True, True), BOARD_SUMMARY),
    0x02: PacketType("TeleMini sensor", _sensor(False, True), BOARD_SUMMARY),
    0x03: PacketType("TeleNano sensor", _sensor(False, False), BOARD_SUMMARY),
    0x04: PacketType("Configuration", CONFIGURATION,
                     ("device_name", "callsign", "version", "flight"),
                     _post_configuration),
    0x05: PacketType("GPS location", LOCATION,
                     ("locked", "nsat", "latitude", "longitude"), _post_location),
    0x06: PacketType("GPS satellites", (
        F("channels", 5, "B", "Satellites reported"),
        F("sats", 6, "24s", "Satellites (id, C/N₀)", "", lambda raw: None),
    ), ("channels",), _post_satellite),
    0x07: PacketType("Companion", (
        F("board_id", 5, "B", "Board ID"),
        F("update_period", 6, "B", "Update period"),
        F("channels", 7, "B", "Channels"),
        F("data", 8, "24s", "Channel data", "", lambda raw: None),
    ), ("board_id", "channels"), _post_companion),
    0x08: PacketType("Mega sensor (MPU)", MEGA_SENSOR_MPU,
                     ("pressure", "temperature", "orient"), _post_mega_sensor_mpu),
    0x09: PacketType("Mega data (15 V)", _mega_data(_adc12_pyro_15v), BOARD_SUMMARY),
    0x0a: PacketType("Metrum sensor", METRUM_SENSOR, BOARD_SUMMARY),
    0x0b: PacketType("Metrum data", METRUM_DATA, ("ground_pressure",)),
    0x10: PacketType("TeleMini 2", _mini(_mini2_volts, _mini2_volts), BOARD_SUMMARY),
    0x11: PacketType("TeleMini 3", _mini(_adc12_battery, _adc12_pyro_15v), BOARD_SUMMARY),
    0x12: PacketType("Mega sensor (BMX160)", MEGA_SENSOR_BMX160,
                     ("pressure", "temperature", "orient")),
    0x13: PacketType("Mega norm (MPU6000)", _mega_norm(2048.0, MPU_COUNTS_PER_DPS, 4096.0),
                     ("pressure", "temperature", "orient")),
    0x14: PacketType("Mega norm (BMI088)", _mega_norm(1365.0, 16.384, 4096.0),
                     ("pressure", "temperature", "orient")),
    0x15: PacketType("Mega data (30 V)", _mega_data(_adc12_pyro_30v), BOARD_SUMMARY),
    0x80: PacketType("Baro samples", (
        F("samples", 5, "B", "Samples"),
        F("baro", 6, "24s", "Pressure samples", "counts", lambda raw: None),
    ), ("samples",), _post_baro),
}


def schema():
    """Describe every packet type so the page can label fields and bytes."""
    return {
        "states": FLIGHT_STATES,
        "igniterGoodVolts": IGNITER_GOOD_VOLTS,
        "header": [_field_schema(f) for f in HEADER],
        "types": {
            str(type_id): {
                "name": t.name,
                "summary": t.summary,
                "fields": [_field_schema(f) for f in t.fields],
            }
            for type_id, t in TYPES.items()
        },
    }


def _field_schema(f):
    return {"name": f.name, "label": f.label, "unit": f.unit,
            "offset": f.offset, "size": f.size}


def decode(raw):
    """Decode one 32-byte packet. Unknown types decode to header fields only."""
    if len(raw) != PACKET_SIZE:
        raise ValueError("expected %d bytes, got %d" % (PACKET_SIZE, len(raw)))
    serial, tick, type_id = struct.unpack_from("<HHB", raw, 0)
    packet_type = TYPES.get(type_id)
    fields = {}
    if packet_type:
        for f in packet_type.fields:
            value = struct.unpack_from("<" + f.fmt, raw, f.offset)[0]
            fields[f.name] = f.conv(value) if f.conv else value
        if packet_type.post:
            packet_type.post(fields, raw, serial)
    return {
        "serial": serial,
        "tick": tick,
        "type": type_id,
        "known": packet_type is not None,
        "f": {k: round(v, 5) if isinstance(v, float) else v for k, v in fields.items()},
    }


class TickClock:
    """Turn each device's 16-bit tick into seconds that keep counting past the
    wrap at 655.36 s. Telemetry ticks can arrive slightly out of order (GPS
    packets carry older ticks), so changes are read as signed. Receive time
    fills in any wraps missed during a long radio dropout.
    """

    WRAP = 1 << 16

    def __init__(self):
        self._last = {}

    def seconds(self, serial, tick, received_s):
        last = self._last.get(serial)
        if last is None:
            total = tick
        else:
            total_last, received_last = last
            delta = ((tick - total_last + self.WRAP // 2) % self.WRAP) - self.WRAP // 2
            elapsed = (received_s - received_last) * TICKS_PER_SECOND
            total = total_last + delta + self.WRAP * round((elapsed - delta) / self.WRAP)
        if last is None or total >= last[0]:
            self._last[serial] = (total, received_s)
        return total / TICKS_PER_SECOND


_HEX_LINE = re.compile(r"^(?:packet: )?((?:[0-9a-f]{2} ?){%d,})$" % PACKET_SIZE)
# One firmware build printed no newline after the CRC_OK line, so hex follows it.
_INLINE_HEX = re.compile(r"([0-9a-f]{2}(?: [0-9a-f]{2}){%d,}) ?$" % (PACKET_SIZE - 1))
_RSSI_LINE = re.compile(r"^rssi: (-?\d+(?:\.\d+)?)$")
_START_LINE = re.compile(r"^(?:packet rx success$|CRC_OK: )")
_END_LINE = re.compile(r"^-{5,}$")

# The radio appends two status bytes after the packet: RSSI, then this one.
STATUS_OFFSET = PACKET_SIZE + 1
STATUS_CRC_OK = 0x80
STATUS_LQI_MASK = 0x7f


class LineParser:
    """Reassemble packets from the firmware's console lines (main.cpp).

    Handles every format in logs/: the older one (hex, then rssi), and the
    current one (CRC_OK, rssi, then a longer hex line with status bytes).

    The hex dump is the source of truth. CRC and LQI come from the status byte,
    not the printed CRC_OK line: bit 7 is CRC OK and bits 6:0 are LQI, whereas
    the printed values come from a bitfield declared the other way around.
    Bytes past the status byte are uninitialised firmware memory.
    """

    def __init__(self):
        self._raw = None
        self._rssi = None

    def feed(self, line):
        """Take one line. Returns a finished (raw, rssi, crc_ok, lqi) or None.

        crc_ok and lqi are None for logs without status bytes.
        """
        line = line.strip()
        if _END_LINE.match(line):
            return self._finish()
        if _START_LINE.match(line):
            finished = self._finish()
            match = _INLINE_HEX.search(line)
            if match:
                self._raw = bytes.fromhex(match.group(1))
            return finished
        match = _HEX_LINE.match(line)
        if match:
            self._raw = bytes.fromhex(match.group(1))
            return None
        match = _RSSI_LINE.match(line)
        if match:
            rssi = float(match.group(1))
            self._rssi = None if rssi == INVALID_RSSI else rssi
        return None

    def _finish(self):
        finished = None
        if self._raw is not None:
            crc_ok = lqi = None
            if len(self._raw) > STATUS_OFFSET:
                status = self._raw[STATUS_OFFSET]
                crc_ok = bool(status & STATUS_CRC_OK)
                lqi = status & STATUS_LQI_MASK
            finished = (self._raw[:PACKET_SIZE], self._rssi, crc_ok, lqi)
        self._raw = None
        self._rssi = None
        return finished
