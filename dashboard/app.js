"use strict";

// Everything here is local on purpose: launch sites often have no internet, so
// charts are drawn by hand rather than pulled from a CDN, and the GPS track has
// no map tiles behind it.

const CONFIRMED_AFTER = 3;    // valid packets before a serial counts as a device
const GAP_SECONDS = 5;        // break a line across dropouts longer than this
const STALE_SECONDS = 5;      // warn when nothing has arrived for this long
const MAX_ROWS = 200;
const MAX_FEED = 20000;
const RATE_WINDOW_MS = 10000;
const CN0_FULL_SCALE = 50;
const FLIGHT_STEPS = ["pad", "boost", "fast", "coast", "drogue", "main", "landed"];
const RANGES = [["1m", 60], ["5m", 300], ["15m", 900], ["All", Infinity]];

const CHARTS = [
  { id: "height", title: "Height", unit: "m", digits: 0, minSpan: 20, flight: true,
    series: [{ key: "height" }] },
  { id: "speed", title: "Speed", unit: "m/s", digits: 1, minSpan: 5, series: [{ key: "speed" }] },
  { id: "acceleration", title: "Acceleration", unit: "m/s²", digits: 1, minSpan: 5,
    series: [{ key: "acceleration" }] },
  { id: "pressure", title: "Pressure", unit: "kPa", digits: 2, minSpan: 0.2,
    series: [{ key: "pressure", scale: 0.001 }] },
  { id: "temperature", title: "Temperature", unit: "°C", digits: 1, minSpan: 2,
    series: [{ key: "temperature" }] },
  { id: "power", title: "Batteries", unit: "V", digits: 2, minSpan: 0.5,
    series: [{ key: "battery", label: "Main" }, { key: "v_pyro", label: "Pyro" }] },
  { id: "rssi", title: "Signal strength", unit: "dBm", digits: 0, minSpan: 10, series: [{ key: "rssi" }] },
  { id: "lqi", title: "Link quality, lower is better", unit: "", digits: 0, minSpan: 10,
    series: [{ key: "lqi" }] },
  { id: "imu-accel", title: "IMU acceleration", unit: "m/s²", digits: 1, minSpan: 5, series: [
    { key: "accel_along", label: "Along" }, { key: "accel_across", label: "Across" },
    { key: "accel_through", label: "Through" }] },
  { id: "orient", title: "Angle from vertical", unit: "°", digits: 0, minSpan: 10,
    series: [{ key: "orient" }] },
  { id: "mag", title: "Magnetometer", unit: "G", digits: 2, minSpan: 0.2, series: [
    { key: "mag_along", label: "Along" }, { key: "mag_across", label: "Across" },
    { key: "mag_through", label: "Through" }] },
];

const PYRO_CHANNELS = [
  ["apogee_v", "Apogee"], ["main_v", "Main"], ["igniter_a", "A"],
  ["igniter_b", "B"], ["igniter_c", "C"], ["igniter_d", "D"],
];

const $ = (id) => document.getElementById(id);

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

let schema = null;
let colors = {};
let link = { state: "starting", detail: "" };
let serverLost = false;
const devices = new Map();
let feed = [];
let selectedSerial = null;
let selectedSeq = null;
let chipFilter = "all";
let rangeSeconds = 60;
let hoverT = null;
let inspectedSeq = null;

// --- Data ------------------------------------------------------------------

class Device {
  constructor(serial) {
    this.serial = serial;
    this.valid = 0;
    this.crcFailures = 0;
    this.series = {};
    this.byType = new Map();
    this.states = [];
    this.fix = [];
    this.rxTimes = [];
    this.peaks = {};
    this.lowRssi = null;
    this.latest = {};
    this.config = null;
    this.location = null;
    this.satellites = null;
    this.firstT = null;
    this.lastT = null;
    this.lastRx = 0;
  }

  get confirmed() { return this.valid >= CONFIRMED_AFTER; }

  push(key, t, value) {
    const s = this.series[key] ||= { t: [], v: [] };
    // GPS packets carry older ticks, so an arrival can land just before the end.
    let at = s.t.length;
    while (at > 0 && s.t[at - 1] > t) at--;
    if (at === s.t.length) { s.t.push(t); s.v.push(value); }
    else { s.t.splice(at, 0, t); s.v.splice(at, 0, value); }
  }

  ingest(p) {
    const f = p.f;
    this.firstT = this.firstT === null ? p.t : Math.min(this.firstT, p.t);
    this.lastT = this.lastT === null ? p.t : Math.max(this.lastT, p.t);
    this.lastRx = p.rx;
    this.rxTimes.push(p.rx);
    if (p.crc_ok === false) this.crcFailures++;
    if (!p.known || p.crc_ok === false) return;
    this.valid++;

    if (!this.byType.has(p.type)) this.byType.set(p.type, []);
    this.byType.get(p.type).push(p.t);

    if (p.rssi !== null) {
      this.push("rssi", p.t, p.rssi);
      this.lowRssi = this.lowRssi === null ? p.rssi : Math.min(this.lowRssi, p.rssi);
    }
    if (p.lqi !== null) this.push("lqi", p.t, p.lqi);
    for (const chart of CHARTS) {
      for (const s of chart.series) {
        const value = f[s.key];
        if (typeof value === "number") this.push(s.key, p.t, value * (s.scale ?? 1));
      }
    }
    for (const key of ["height", "speed", "acceleration"]) {
      if (typeof f[key] === "number") this.peaks[key] = Math.max(this.peaks[key] ?? -Infinity, f[key]);
    }
    for (const [key, value] of Object.entries(f)) this.latest[key] = value;

    if (typeof f.state === "number" && f.state < schema.states.length) {
      const previous = this.states[this.states.length - 1];
      if (!previous || previous.state !== f.state) this.states.push({ t: p.t, state: f.state });
    }
    if (p.type === 4) this.config = f;
    if (p.type === 5) {
      this.location = { ...f, rx: p.rx };
      if (f.locked) this.fix.push({ t: p.t, rx: p.rx, lat: f.latitude, lon: f.longitude, alt: f.altitude });
    }
    if (p.type === 6) this.satellites = f.sats;
  }

  rate() {
    const since = Date.now() - RATE_WINDOW_MS;
    let i = this.rxTimes.length;
    while (i > 0 && this.rxTimes[i - 1] >= since) i--;
    const count = this.rxTimes.length - i;
    return count < 2 ? 0 : count / (RATE_WINDOW_MS / 1000);
  }

  state() {
    const last = this.states[this.states.length - 1];
    return last ? schema.states[last.state] : null;
  }
}

function ingest(p) {
  let device = devices.get(p.serial);
  if (!device) devices.set(p.serial, device = new Device(p.serial));
  device.ingest(p);
  feed.push(p);
}

function currentDevice() {
  const confirmed = [...devices.values()].filter((d) => d.confirmed);
  if (selectedSerial !== null && devices.get(selectedSerial)?.confirmed) return devices.get(selectedSerial);
  if (!confirmed.length) return null;
  const busiest = confirmed.reduce((a, b) => (b.valid > a.valid ? b : a));
  selectedSerial = busiest.serial;
  return busiest;
}

function typeName(type) {
  return schema.types[type]?.name ?? `Unknown 0x${type.toString(16).padStart(2, "0")}`;
}

function isSuspect(p) {
  return !p.known || p.crc_ok === false || !devices.get(p.serial)?.confirmed;
}

// --- Formatting ------------------------------------------------------------

function formatUptime(seconds) {
  const whole = Math.floor(Math.abs(seconds));
  const h = Math.floor(whole / 3600);
  const m = Math.floor(whole / 60) % 60;
  const s = String(whole % 60).padStart(2, "0");
  const text = h ? `${h}:${String(m).padStart(2, "0")}:${s}` : `${m}:${s}`;
  return seconds < 0 ? `−${text}` : text;
}

function formatNumber(value, digits) {
  return value.toLocaleString(undefined, { minimumFractionDigits: digits, maximumFractionDigits: digits })
    .replace("-", "−");
}

function formatClock(ms) {
  const d = new Date(ms);
  const pad = (n, width = 2) => String(n).padStart(width, "0");
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad(d.getMilliseconds(), 3)}`;
}

function digitsFor(field) {
  if (field.unit === "°" && /itude/.test(field.name)) return 7;
  if (field.unit === "V" || field.unit === "G") return 2;
  if (["m/s²", "m/s", "°/s", "°C"].includes(field.unit)) return 2;
  if (/dop$/.test(field.name)) return 1;
  return 0;
}

function formatField(field, value) {
  if (value === null || value === undefined) return "—";
  if (field.name === "state") return schema.states[value] ?? `? (${value})`;
  if (field.name === "flags" || field.name === "mode") return `0x${value.toString(16).padStart(2, "0")}`;
  if (field.name === "device") return `0x${value.toString(16).padStart(2, "0")}`;
  if (field.name === "sats") {
    return value.length ? value.map((s) => `${s.svid}: ${s.cn0} dB-Hz`).join(", ") : "None";
  }
  if (Array.isArray(value)) return value.length ? value.join(", ") : "None";
  if (typeof value === "number") {
    const raw = field.unit === "" || field.unit === "counts" || ["serial", "tick", "flight"].includes(field.name);
    const text = raw && digitsFor(field) === 0 ? String(value) : formatNumber(value, digitsFor(field));
    return field.unit ? `${text} ${field.unit}` : text;
  }
  return String(value);
}

function summarize(p) {
  if (p.crc_ok === false) return "CRC failed";
  const type = schema.types[p.type];
  if (!type) return "Not an AltOS packet type";
  const parts = [];
  for (const name of type.summary) {
    const value = p.f[name];
    if (name === "locked") parts.push(value ? "Locked" : "No fix");
    else if (name === "nsat") parts.push(`${value} sats`);
    else if (name === "channels") parts.push(`${value} channels`);
    else if (name === "flight") parts.push(`flight ${value}`);
    else if (name === "device_name") parts.push(value);
    else if (value !== null && value !== undefined) {
      const field = type.fields.find((f) => f.name === name);
      parts.push(field ? formatField(field, value) : String(value));
    }
  }
  return parts.join(" · ");
}

// --- Canvas helpers --------------------------------------------------------

function readColors() {
  const style = getComputedStyle(document.documentElement);
  for (const name of ["surface", "ink", "muted", "grid", "axis", "series-1", "series-2", "series-3"]) {
    colors[name] = style.getPropertyValue(`--${name}`).trim();
  }
}

function fitCanvas(canvas, cssHeight) {
  const dpr = window.devicePixelRatio || 1;
  const width = canvas.clientWidth;
  const height = cssHeight ?? canvas.clientHeight;
  const pixelWidth = Math.round(width * dpr);
  const pixelHeight = Math.round(height * dpr);
  if (canvas.width !== pixelWidth || canvas.height !== pixelHeight) {
    canvas.width = pixelWidth;
    canvas.height = pixelHeight;
  }
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);
  return { ctx, width, height };
}

function lowerBound(values, target) {
  let lo = 0, hi = values.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (values[mid] < target) lo = mid + 1; else hi = mid;
  }
  return lo;
}

function niceStep(span, count) {
  const raw = span / count;
  const magnitude = 10 ** Math.floor(Math.log10(raw));
  const ratio = raw / magnitude;
  return (ratio < 1.5 ? 1 : ratio < 3 ? 2 : ratio < 7 ? 5 : 10) * magnitude;
}

function timeStep(span, width) {
  const wanted = span / Math.max(2, Math.floor(width / 76));
  const steps = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 14400];
  return steps.find((s) => s >= wanted) ?? 28800;
}

function viewWindow(device) {
  const end = device.lastT;
  let start = rangeSeconds === Infinity ? device.firstT : end - rangeSeconds;
  if (end - start < 1) start = end - 1;
  return [start, end];
}

function drawTimeAxis(ctx, x0, x1, top, bottom, t0, t1) {
  const step = timeStep(t1 - t0, x1 - x0);
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillStyle = colors.muted;
  ctx.font = "11px system-ui, sans-serif";
  ctx.strokeStyle = colors.grid;
  ctx.lineWidth = 1;
  for (let t = Math.ceil(t0 / step) * step; t <= t1; t += step) {
    const x = Math.round(x0 + (t - t0) / (t1 - t0) * (x1 - x0)) + 0.5;
    ctx.beginPath();
    ctx.moveTo(x, top);
    ctx.lineTo(x, bottom);
    ctx.stroke();
    if (x > x0 + 18 && x < x1 - 18) ctx.fillText(formatUptime(t), x, bottom + 4);
  }
}

function drawCrosshair(ctx, x0, x1, top, bottom, t0, t1) {
  if (hoverT === null || hoverT < t0 || hoverT > t1) return;
  const x = Math.round(x0 + (hoverT - t0) / (t1 - t0) * (x1 - x0)) + 0.5;
  ctx.strokeStyle = colors.muted;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(x, top);
  ctx.lineTo(x, bottom);
  ctx.stroke();
}

// --- Time-series charts ----------------------------------------------------

const chartViews = [];

function buildCharts() {
  const container = $("charts");
  for (const chart of CHARTS) {
    const card = el("article", "chart");
    const head = el("div", "chart-head");
    const title = el("h2", null, chart.unit ? `${chart.title} (${chart.unit})` : chart.title);
    const now = el("span", "chart-now");
    head.append(title, now);
    card.append(head);
    const legend = el("div", "legend");
    chart.series.forEach((s, i) => {
      if (chart.series.length < 2) return;
      const item = el("span");
      const swatch = el("i");
      swatch.style.background = `var(--series-${i + 1})`;
      item.append(swatch, s.label);
      legend.append(item);
    });
    card.append(legend);
    const canvas = el("canvas");
    canvas.setAttribute("role", "img");
    canvas.setAttribute("aria-label", `${chart.title} over time`);
    card.append(canvas);
    container.append(card);
    const view = { chart, canvas, now, geometry: null };
    chartViews.push(view);
    canvas.addEventListener("pointermove", (event) => onChartHover(view, event));
    canvas.addEventListener("pointerleave", clearHover);
    if (chart.id === "orient") container.append(buildTiltDial());
  }
}

function buildTiltDial() {
  const card = el("article", "chart");
  const head = el("div", "chart-head");
  head.append(el("h2", null, "Rocket tilt"), Object.assign(el("span", "chart-now"), { id: "tilt-now" }));
  const canvas = el("canvas");
  canvas.id = "tilt";
  canvas.setAttribute("role", "img");
  card.append(head, el("div", "legend", "Tilt only: orient doesn't say which way it leans"), canvas);
  return card;
}

function drawTiltDial(device) {
  const canvas = $("tilt");
  const { ctx, width, height } = fitCanvas(canvas);
  const degrees = device?.latest.orient;
  const hasData = typeof degrees === "number";
  $("tilt-now").textContent = hasData ? `${degrees}°` : "";
  canvas.setAttribute("aria-label", hasData ? `Tilted ${degrees} degrees from vertical` : "No tilt data");
  ctx.font = "11px system-ui, sans-serif";
  if (!hasData) {
    ctx.fillStyle = colors.muted;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.font = "13px system-ui, sans-serif";
    ctx.fillText("No data from this device", width / 2, height / 2);
    return;
  }

  const cx = width / 2, cy = height / 2;
  const radius = height / 2 - 18;
  const angle = Math.min(180, degrees) * Math.PI / 180;  // clockwise from straight up

  ctx.strokeStyle = colors.grid;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.arc(cx, cy, radius, 0, Math.PI * 2);
  ctx.moveTo(cx, cy - radius);
  ctx.lineTo(cx, cy + radius);
  ctx.stroke();

  ctx.fillStyle = colors.muted;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  for (const mark of [0, 45, 90, 135, 180]) {
    const a = mark * Math.PI / 180;
    ctx.strokeStyle = colors.axis;
    ctx.beginPath();
    ctx.moveTo(cx + Math.sin(a) * (radius - 4), cy - Math.cos(a) * (radius - 4));
    ctx.lineTo(cx + Math.sin(a) * (radius + 4), cy - Math.cos(a) * (radius + 4));
    ctx.stroke();
    ctx.fillText(`${mark}°`, cx + Math.sin(a) * (radius + 15), cy - Math.cos(a) * (radius + 11));
  }

  ctx.strokeStyle = colors["series-1"];
  ctx.lineWidth = 3;
  ctx.lineCap = "round";
  ctx.beginPath();
  ctx.arc(cx, cy, radius, -Math.PI / 2, angle - Math.PI / 2);
  ctx.stroke();

  // The rocket, nose up in its own frame, then turned by the tilt.
  const half = radius * 0.72;
  const body = radius * 0.13;
  const nose = radius * 0.3;
  ctx.save();
  ctx.translate(cx, cy);
  ctx.rotate(angle);
  ctx.fillStyle = colors["series-1"];
  ctx.beginPath();
  ctx.moveTo(0, -half);
  ctx.quadraticCurveTo(body, -half + nose * 0.5, body, -half + nose);
  ctx.lineTo(body, half - radius * 0.2);
  ctx.lineTo(body * 2, half);
  ctx.lineTo(body, half - radius * 0.06);
  ctx.lineTo(-body, half - radius * 0.06);
  ctx.lineTo(-body * 2, half);
  ctx.lineTo(-body, half - radius * 0.2);
  ctx.lineTo(-body, -half + nose);
  ctx.quadraticCurveTo(-body, -half + nose * 0.5, 0, -half);
  ctx.fill();
  ctx.restore();
}

function drawChart(view, device) {
  const { chart, canvas } = view;
  const { ctx, width, height } = fitCanvas(canvas);
  const left = 46, right = width - 8, top = 6, bottom = height - 20;
  view.geometry = null;
  if (!device || width === 0) return;

  const present = chart.series
    .map((s, i) => ({ ...s, color: colors[`series-${i + 1}`], data: device.series[s.key] }))
    .filter((s) => s.data);
  view.now.textContent = present.length
    ? present.map((s) => formatNumber(s.data.v[s.data.v.length - 1], chart.digits)).join(" / ")
    : "";

  if (!present.length) {
    ctx.fillStyle = colors.muted;
    ctx.font = "13px system-ui, sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("No data from this device", width / 2, height / 2);
    return;
  }

  const [t0, t1] = viewWindow(device);
  let min = Infinity, max = -Infinity;
  for (const s of present) {
    s.from = Math.max(0, lowerBound(s.data.t, t0) - 1);
    s.to = lowerBound(s.data.t, t1 + 0.001);
    for (let i = s.from; i < s.to; i++) {
      min = Math.min(min, s.data.v[i]);
      max = Math.max(max, s.data.v[i]);
    }
  }
  if (min === Infinity) { min = 0; max = 1; }
  if (max - min < chart.minSpan) {
    const middle = (max + min) / 2;
    min = middle - chart.minSpan / 2;
    max = middle + chart.minSpan / 2;
  }
  const step = niceStep(max - min, 4);
  const low = Math.floor(min / step) * step;
  const high = Math.ceil(max / step) * step;
  const xOf = (t) => left + (t - t0) / (t1 - t0) * (right - left);
  const yOf = (v) => bottom - (v - low) / (high - low) * (bottom - top);
  view.geometry = { left, right, t0, t1, present };

  ctx.font = "11px system-ui, sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  ctx.lineWidth = 1;
  const stepDigits = Math.max(0, -Math.floor(Math.log10(step)));
  for (let v = low, i = 0; v <= high + step / 2; v = low + ++i * step) {
    const y = Math.round(yOf(v)) + 0.5;
    ctx.strokeStyle = Math.abs(v) < step / 2 ? colors.axis : colors.grid;
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(right, y);
    ctx.stroke();
    ctx.fillStyle = colors.muted;
    ctx.fillText(formatNumber(v, stepDigits), left - 6, y);
  }
  drawTimeAxis(ctx, left, right, top, bottom, t0, t1);

  const labelEnds = [-Infinity, -Infinity, -Infinity];  // staggered so close states stay readable
  for (const { t, state } of device.states) {
    if (t < t0 || t > t1 || !FLIGHT_STEPS.includes(schema.states[state])) continue;
    if (schema.states[state] === "pad") continue;
    const x = Math.round(xOf(t)) + 0.5;
    ctx.strokeStyle = colors.axis;
    ctx.beginPath();
    ctx.moveTo(x, top);
    ctx.lineTo(x, bottom);
    ctx.stroke();
    if (chart.flight) {
      const name = schema.states[state];
      const row = labelEnds.findIndex((end) => end < x);
      if (row >= 0) {
        ctx.fillStyle = colors.muted;
        ctx.textAlign = "left";
        ctx.textBaseline = "top";
        ctx.fillText(name, x + 3, top + 1 + row * 13);
        labelEnds[row] = x + 3 + ctx.measureText(name).width + 4;
      }
    }
  }

  ctx.save();
  ctx.beginPath();
  ctx.rect(left, top - 2, right - left + 4, bottom - top + 4);
  ctx.clip();
  for (const s of present) drawLine(ctx, s, xOf, yOf);
  ctx.restore();

  for (const s of present) {
    if (s.to === 0 || s.data.t[s.to - 1] < t0) continue;
    const x = xOf(s.data.t[s.to - 1]), y = yOf(s.data.v[s.to - 1]);
    ctx.beginPath();
    ctx.arc(x, y, 3.5, 0, Math.PI * 2);
    ctx.fillStyle = s.color;
    ctx.strokeStyle = colors.surface;
    ctx.lineWidth = 1.5;
    ctx.fill();
    ctx.stroke();
  }
  drawCrosshair(ctx, left, right, top, bottom, t0, t1);
}

function drawLine(ctx, s, xOf, yOf) {
  const { t, v } = s.data;
  ctx.strokeStyle = s.color;
  ctx.lineWidth = 2;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  let bucketX = null, bucketLow = 0, bucketHigh = 0, previousT = null;
  const flush = () => {
    if (bucketX !== null && bucketLow !== bucketHigh) {
      ctx.lineTo(bucketX, yOf(bucketLow));
      ctx.lineTo(bucketX, yOf(bucketHigh));
    }
  };
  // Several samples in one pixel column collapse to their min and max, so a
  // long flight stays fast to draw without hiding spikes.
  for (let i = s.from; i < s.to; i++) {
    const x = Math.round(xOf(t[i]));
    const broken = previousT === null || t[i] - previousT > GAP_SECONDS;
    if (broken) {
      flush();
      ctx.moveTo(x, yOf(v[i]));
      ctx.lineTo(x + 0.01, yOf(v[i]));
      bucketX = x; bucketLow = bucketHigh = v[i];
    } else if (x !== bucketX) {
      flush();
      ctx.lineTo(x, yOf(v[i]));
      bucketX = x; bucketLow = bucketHigh = v[i];
    } else {
      bucketLow = Math.min(bucketLow, v[i]);
      bucketHigh = Math.max(bucketHigh, v[i]);
    }
    previousT = t[i];
  }
  flush();
  ctx.stroke();
}

function onChartHover(view, event) {
  const g = view.geometry;
  if (!g) return clearHover();
  const rect = view.canvas.getBoundingClientRect();
  const x = Math.min(g.right, Math.max(g.left, event.clientX - rect.left));
  hoverT = g.t0 + (x - g.left) / (g.right - g.left) * (g.t1 - g.t0);

  const lines = [];
  for (const s of g.present) {
    const at = lowerBound(s.data.t, hoverT);
    const candidates = [at - 1, at].filter((i) => i >= 0 && i < s.data.t.length);
    if (!candidates.length) continue;
    const nearest = candidates.reduce((a, b) =>
      Math.abs(s.data.t[b] - hoverT) < Math.abs(s.data.t[a] - hoverT) ? b : a);
    if (Math.abs(s.data.t[nearest] - hoverT) > GAP_SECONDS) continue;
    lines.push({ color: s.color, label: s.label ?? view.chart.title,
      value: `${formatNumber(s.data.v[nearest], view.chart.digits)} ${view.chart.unit}`.trim() });
  }
  showTooltip(event, formatUptime(hoverT), lines);
  requestDraw();
}

function clearHover() {
  hoverT = null;
  $("tooltip").hidden = true;
  requestDraw();
}

function showTooltip(event, heading, lines) {
  const tip = $("tooltip");
  tip.replaceChildren();
  const head = el("div", "head");
  head.textContent = heading;
  tip.append(head);
  if (!lines.length) tip.append(el("div", null, "No data here"));
  for (const line of lines) {
    const row = el("div");
    const label = el("span");
    if (line.color) {
      const swatch = el("i");
      swatch.style.background = line.color;
      label.append(swatch);
    }
    label.append(line.label);
    row.append(label, el("b", null, line.value));
    tip.append(row);
  }
  tip.hidden = false;
  const box = tip.getBoundingClientRect();
  const x = event.clientX + 14 + box.width > innerWidth ? event.clientX - box.width - 14 : event.clientX + 14;
  tip.style.left = `${Math.max(8, x)}px`;
  tip.style.top = `${Math.max(8, Math.min(innerHeight - box.height - 8, event.clientY + 12))}px`;
}

// --- Packet timeline -------------------------------------------------------

function drawTimeline(device) {
  const canvas = $("timeline");
  const types = device ? [...device.byType.keys()].sort((a, b) => a - b) : [];
  const laneHeight = 22;
  const cssHeight = Math.max(1, types.length) * laneHeight + 24;
  canvas.style.height = `${cssHeight}px`;
  const { ctx, width } = fitCanvas(canvas, cssHeight);
  const left = Math.min(170, width * 0.4), right = width - 8, top = 2;
  const bottom = top + types.length * laneHeight;
  canvas.lanes = null;
  if (!types.length) {
    ctx.fillStyle = colors.muted;
    ctx.font = "13px system-ui, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText("Waiting for packets", width / 2, cssHeight / 2);
    return;
  }
  const [t0, t1] = viewWindow(device);
  const xOf = (t) => left + (t - t0) / (t1 - t0) * (right - left);
  drawTimeAxis(ctx, left, right, top, bottom, t0, t1);
  ctx.font = "12px system-ui, sans-serif";
  types.forEach((type, lane) => {
    const y = top + lane * laneHeight;
    ctx.fillStyle = colors.muted;
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    ctx.fillText(typeName(type), left - 8, y + laneHeight / 2, left - 12);
    ctx.fillStyle = colors["series-1"];
    const times = device.byType.get(type);
    let lastX = null;
    for (let i = lowerBound(times, t0); i < times.length; i++) {
      const x = Math.round(xOf(times[i]));
      if (x !== lastX) ctx.fillRect(x - 1, y + 4, 2, laneHeight - 8);
      lastX = x;
    }
  });
  canvas.lanes = { types, laneHeight, top, bottom, left, right, t0, t1 };
  drawCrosshair(ctx, left, right, top, bottom, t0, t1);
}

function onTimelineHover(event) {
  const canvas = $("timeline");
  const lanes = canvas.lanes;
  const device = currentDevice();
  if (!lanes || !device) return;
  const rect = canvas.getBoundingClientRect();
  const x = Math.min(lanes.right, Math.max(lanes.left, event.clientX - rect.left));
  const lane = Math.floor((event.clientY - rect.top - lanes.top) / lanes.laneHeight);
  hoverT = lanes.t0 + (x - lanes.left) / (lanes.right - lanes.left) * (lanes.t1 - lanes.t0);
  const type = lanes.types[Math.max(0, Math.min(lanes.types.length - 1, lane))];
  const times = device.byType.get(type);
  const inView = times.length - lowerBound(times, lanes.t0);
  showTooltip(event, formatUptime(hoverT), [{ label: typeName(type), value: `${inView} in view` }]);
  requestDraw();
}

// --- GPS -------------------------------------------------------------------

const METERS_PER_DEGREE = 111320;

function enu(origin, point) {
  return {
    east: (point.lon - origin.lon) * METERS_PER_DEGREE * Math.cos(origin.lat * Math.PI / 180),
    north: (point.lat - origin.lat) * METERS_PER_DEGREE,
  };
}

function drawTrack(device) {
  const canvas = $("track");
  const { ctx, width, height } = fitCanvas(canvas);
  ctx.font = "12px system-ui, sans-serif";
  if (!device || !device.fix.length) {
    ctx.fillStyle = colors.muted;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(device?.location?.running ? "GPS running, no fix yet" : "No GPS data yet", width / 2, height / 2);
    return;
  }
  const origin = device.fix[0];
  const points = device.fix.map((p) => enu(origin, p));
  const extent = Math.max(50, ...points.map((p) => Math.max(Math.abs(p.east), Math.abs(p.north))));
  const scale = (Math.min(width, height) / 2 - 18) / extent;
  const cx = width / 2, cy = height / 2;
  const xOf = (p) => cx + p.east * scale;
  const yOf = (p) => cy - p.north * scale;

  ctx.strokeStyle = colors.grid;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, Math.round(cy) + 0.5);
  ctx.lineTo(width, Math.round(cy) + 0.5);
  ctx.moveTo(Math.round(cx) + 0.5, 0);
  ctx.lineTo(Math.round(cx) + 0.5, height);
  ctx.stroke();

  ctx.strokeStyle = colors["series-1"];
  ctx.lineWidth = 2;
  ctx.lineJoin = "round";
  ctx.beginPath();
  points.forEach((p, i) => (i ? ctx.lineTo(xOf(p), yOf(p)) : ctx.moveTo(xOf(p), yOf(p))));
  ctx.stroke();

  const dot = (p, fill) => {
    ctx.beginPath();
    ctx.arc(xOf(p), yOf(p), 4.5, 0, Math.PI * 2);
    ctx.fillStyle = fill;
    ctx.strokeStyle = colors.surface;
    ctx.lineWidth = 2;
    ctx.fill();
    ctx.stroke();
  };
  dot(points[0], colors.muted);
  dot(points[points.length - 1], colors["series-1"]);

  ctx.fillStyle = colors.muted;
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillText("N", cx + 8, 4);

  const barMeters = niceStep(extent, 1);
  const barPixels = barMeters * scale;
  ctx.strokeStyle = colors.muted;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(10, height - 10);
  ctx.lineTo(10 + barPixels, height - 10);
  ctx.stroke();
  ctx.textAlign = "left";
  ctx.textBaseline = "bottom";
  ctx.fillText(barMeters >= 1000 ? `${barMeters / 1000} km` : `${barMeters} m`, 10, height - 13);
}

function bearing(origin, point) {
  const { east, north } = enu(origin, point);
  return { distance: Math.hypot(east, north), degrees: (Math.atan2(east, north) * 180 / Math.PI + 360) % 360 };
}

function renderGps(device) {
  const facts = $("gps-facts");
  facts.replaceChildren();
  const add = (name, value, primary) => {
    const dd = el("dd", primary ? "primary" : null, value);
    facts.append(el("dt", null, name), dd);
  };
  const location = device?.location;
  const lastFix = device?.fix[device.fix.length - 1];
  add("Status", !location ? "No data" : location.locked ? `Locked, ${location.nsat} sats`
    : location.running ? `No fix, ${location.nsat} sats` : "Not running");
  if (lastFix) {
    add("Last latitude", lastFix.lat.toFixed(7), true);
    add("Last longitude", lastFix.lon.toFixed(7), true);
    add("Fix age", `${Math.max(0, Math.round((device.lastRx - lastFix.rx) / 1000))} s`);
    add("Altitude", `${formatNumber(lastFix.alt, 0)} m`);
    const fromStart = bearing(device.fix[0], lastFix);
    add("From first fix", `${formatNumber(fromStart.distance, 0)} m at ${fromStart.degrees.toFixed(0)}°`);
    if (location.locked) {
      add("Ground speed", `${formatNumber(location.ground_speed, 1)} m/s`);
      add("Climb rate", `${formatNumber(location.climb_rate, 1)} m/s`);
    }
  }
  if (location) add("HDOP / VDOP", `${location.hdop} / ${location.vdop}`);

  const sats = $("sats");
  sats.replaceChildren();
  const list = device?.satellites ?? [];
  if (!list.length) sats.append(el("p", "note", "No satellites reported."));
  for (const sat of [...list].sort((a, b) => a.svid - b.svid)) {
    const row = el("div", "sat");
    row.title = `Satellite ${sat.svid}: C/N₀ ${sat.cn0} dB-Hz`;
    const track = el("div", "sat-track");
    const bar = el("i");
    bar.style.width = `${Math.min(100, sat.cn0 / CN0_FULL_SCALE * 100)}%`;
    track.append(bar);
    row.append(el("span", null, `#${sat.svid}`), track, el("span", null, String(sat.cn0)));
    sats.append(row);
  }
}

// --- Fixed panels ----------------------------------------------------------

function renderHeader(device) {
  const line = $("device-line");
  if (!device) {
    const noise = [...devices.values()].reduce((sum, d) => sum + d.rxTimes.length, 0);
    line.textContent = noise ? "Listening, no confirmed device yet" : "No device yet";
  } else {
    const c = device.config;
    line.textContent = c
      ? `${c.device_name} ${device.serial} · ${c.callsign} · firmware ${c.version} · flight ${c.flight}`
      : `Serial ${device.serial}`;
  }
  const confirmed = [...devices.values()].filter((d) => d.confirmed);
  $("device-picker").hidden = confirmed.length < 2;
  const select = $("device-select");
  const wanted = confirmed.map((d) => String(d.serial)).join();
  if (select.dataset.serials !== wanted) {
    select.replaceChildren(...confirmed.map((d) => {
      const option = el("option", null, `Serial ${d.serial}`);
      option.value = d.serial;
      return option;
    }));
    select.dataset.serials = wanted;
  }
  if (device) select.value = device.serial;
}

function renderLink() {
  const status = $("link");
  const age = $("age");
  const newest = Math.max(0, ...[...devices.values()].map((d) => d.lastRx));
  const seconds = newest ? (Date.now() - newest) / 1000 : null;
  const streaming = link.state === "connected" || link.state === "replaying";
  const stale = streaming && seconds !== null && seconds > STALE_SECONDS;

  let label, tone, message = "";
  if (serverLost) {
    [label, tone] = ["Server disconnected", "critical"];
    message = "Lost contact with server.py. Reconnecting.";
  } else if (link.state === "connected") {
    [label, tone] = [`Live on ${link.detail}`, stale ? "warning" : "good"];
  } else if (link.state === "replaying") {
    [label, tone] = ["Replaying log", stale ? "warning" : "good"];
  } else if (link.state === "finished") {
    [label, tone] = ["Replay finished", "neutral"];
  } else {
    [label, tone] = ["Waiting for receiver", "warning"];
    message = link.detail;
  }
  if (!message && stale) message = `No packets for ${Math.round(seconds)} s.`;
  status.textContent = label;
  status.dataset.tone = tone;
  status.title = link.log ? `Logging to ${link.log}` : "";
  age.textContent = seconds === null ? "No packets yet" : `Last packet ${seconds < 1 ? "under 1" : Math.round(seconds)} s ago`;

  const banner = $("banner");
  banner.hidden = !message;
  banner.textContent = message;
}

function renderStepper(device) {
  const list = $("stepper");
  const state = device?.state();
  const position = FLIGHT_STEPS.indexOf(state);
  list.replaceChildren(...FLIGHT_STEPS.map((name, i) => {
    const item = el("li", position === i ? "current" : position > i ? "done" : null, name);
    if (position === i) item.setAttribute("aria-current", "step");
    return item;
  }));
  if (state && position < 0) {
    list.append(el("li", "current", state));
  }
}

function renderTiles(device) {
  const latest = device?.latest ?? {};
  const peak = (key, unit, digits) => (device?.peaks[key] === undefined ? "No data"
    : `Peak ${formatNumber(device.peaks[key], digits)} ${unit}`);
  const value = (v, digits) => (typeof v === "number" ? formatNumber(v, digits) : "—");
  const rssi = device?.series.rssi;
  const location = device?.location;
  const tiles = [
    ["Height", value(latest.height, 0), "m", peak("height", "m", 0)],
    ["Speed", value(latest.speed, 1), "m/s", peak("speed", "m/s", 1)],
    ["Acceleration", value(latest.acceleration, 1), "m/s²", peak("acceleration", "m/s²", 1)],
    ["Battery", value(latest.battery, 2), "V",
      typeof latest.v_pyro === "number" ? `Pyro ${formatNumber(latest.v_pyro, 2)} V` : "No pyro reading"],
    ["Signal", rssi ? value(rssi.v[rssi.v.length - 1], 0) : "—", "dBm",
      device ? `${device.rate().toFixed(1)} packets/s` + (device.lowRssi === null ? "" : ` · low ${formatNumber(device.lowRssi, 0)} dBm`)
        + (device.crcFailures ? ` · ${device.crcFailures} CRC failed` : "") : "No data"],
    ["GPS", location ? String(location.nsat) : "—", "sats",
      !location ? "No data" : location.locked ? "Locked" : location.running ? "No fix" : "Not running"],
  ];
  const container = $("tiles");
  if (container.children.length !== tiles.length) {
    container.replaceChildren(...tiles.map(() => el("article", "tile")));
  }
  tiles.forEach(([label, number, unit, sub], i) => {
    const tile = container.children[i];
    const big = el("div", "tile-value", number);
    big.append(el("span", "tile-unit", unit));
    tile.replaceChildren(el("div", "tile-label", label), big, el("div", "tile-sub", sub));
  });
}

function renderPyro(device) {
  const panel = $("pyro");
  const latest = device?.latest ?? {};
  const channels = PYRO_CHANNELS.filter(([key]) => typeof latest[key] === "number");
  if (!channels.length) {
    panel.replaceChildren(el("p", "note", "No continuity data from this device."));
    return;
  }
  panel.replaceChildren(...channels.map(([key, label]) => {
    const row = el("div", "pyro-row");
    const ready = latest[key] >= schema.igniterGoodVolts;
    row.append(el("span", null, label), el("span", "pyro-volts", `${formatNumber(latest[key], 2)} V`),
      el("span", ready ? "pyro-state ready" : "pyro-state", ready ? "Ready" : "Open"));
    return row;
  }));
  panel.append(el("p", "note", `Ready means at least ${schema.igniterGoodVolts} V, the threshold AltOS uses.`));
}

// --- Packet list and inspector ---------------------------------------------

function renderChips() {
  const counts = new Map();
  let suspect = 0;
  for (const p of feed) {
    if (isSuspect(p)) suspect++;
    else counts.set(p.type, (counts.get(p.type) ?? 0) + 1);
  }
  const chips = [["all", `All ${feed.length}`]];
  for (const type of [...counts.keys()].sort((a, b) => a - b)) {
    chips.push([String(type), `${typeName(type)} ${counts.get(type)}`]);
  }
  if (suspect) chips.push(["suspect", `Suspect ${suspect}`]);
  // Counts change constantly. Updating in place keeps a click from landing on
  // a button that was replaced between press and release.
  const container = $("chips");
  const values = chips.map(([value]) => value).join();
  if (container.dataset.values !== values) {
    container.dataset.values = values;
    container.replaceChildren(...chips.map(([value]) => {
      const chip = el("button", "chip");
      chip.type = "button";
      chip.addEventListener("click", () => { chipFilter = value; renderStream(true); });
      return chip;
    }));
  }
  chips.forEach(([value, text], i) => {
    const chip = container.children[i];
    chip.textContent = text;
    chip.setAttribute("aria-pressed", String(chipFilter === value));
  });
}

function matchesChip(p) {
  if (chipFilter === "all") return true;
  if (chipFilter === "suspect") return isSuspect(p);
  return String(p.type) === chipFilter && !isSuspect(p);
}

function renderStream(force = false) {
  renderChips();
  // Hold the list still while it is being used, so rows don't move under the
  // pointer and clicks land.
  const inUse = document.querySelector(".table-wrap").matches(":hover, :focus-within");
  $("row-note").classList.toggle("paused", inUse);
  if (inUse && !force) {
    $("row-note").textContent = "List paused while you are using it.";
    return;
  }
  const rows = [];
  let matched = 0;
  for (let i = feed.length - 1; i >= 0; i--) {
    if (!matchesChip(feed[i])) continue;
    matched++;
    if (rows.length < MAX_ROWS) rows.push(feed[i]);
  }
  $("rows").replaceChildren(...rows.map((p) => {
    const suspect = isSuspect(p);
    const row = el("tr", suspect ? "suspect" : null);
    row.tabIndex = 0;
    row.dataset.seq = p.seq;
    row.setAttribute("aria-selected", String(p.seq === selectedSeq));
    row.append(
      el("td", null, formatClock(p.rx)),
      el("td", null, formatUptime(p.t)),
      el("td", suspect ? "flag" : null, typeName(p.type)),
      el("td", "num", String(p.serial)),
      el("td", "num", p.rssi === null ? "—" : formatNumber(p.rssi, 0)),
      el("td", "num", p.lqi === null ? "—" : String(p.lqi)),
      el("td", null, summarize(p)));
    const choose = () => {
      selectedSeq = p.seq;
      for (const other of $("rows").children) {
        other.setAttribute("aria-selected", String(other === row));
      }
      renderInspector();
    };
    row.addEventListener("click", choose);
    row.addEventListener("keydown", (e) => { if (e.key === "Enter") choose(); });
    return row;
  }));
  $("row-note").textContent = matched > MAX_ROWS ? `Showing the newest ${MAX_ROWS} of ${matched}.` : "";
}

function renderInspector() {
  const panel = $("inspector");
  const p = feed.find((candidate) => candidate.seq === selectedSeq) ?? feed[feed.length - 1];
  if (!p) return;
  if (inspectedSeq === p.seq) return;
  inspectedSeq = p.seq;
  const type = schema.types[p.type];
  const fields = [...schema.header, ...(type?.fields ?? [])];
  const owner = new Array(32).fill(-1);
  fields.forEach((field, index) => {
    for (let i = field.offset; i < field.offset + field.size; i++) owner[i] = index;
  });

  panel.replaceChildren(el("p", "inspect-title", `${typeName(p.type)} · ${formatClock(p.rx)}`));
  if (p.crc_ok === false) {
    panel.append(el("p", "note", "CRC failed, so these bytes can't be trusted. It stays out of the charts."));
  } else if (isSuspect(p)) {
    panel.append(el("p", "note", "Suspect: unknown type or an unconfirmed serial, so it stays out of the charts. Likely radio noise."));
  }

  const hex = el("div", "hex");
  const cells = [];
  for (let i = 0; i < 32; i++) {
    const cell = el("span", owner[i] % 2 ? "odd" : null, p.hex.slice(i * 2, i * 2 + 2));
    cell.title = `Byte ${i}${owner[i] >= 0 ? `: ${fields[owner[i]].label}` : ": unused"}`;
    cells.push(cell);
  }
  hex.append(...cells);
  panel.append(hex);

  const list = el("div", "fields");
  const rows = fields.map((field) => {
    const row = el("div", "field");
    const source = schema.header.includes(field) ? p : p.f;
    row.append(el("span", null, field.label), el("span", null, formatField(field, source[field.name])));
    list.append(row);
    return row;
  });
  if (p.crc_ok !== null) {
    for (const [label, value] of [["CRC", p.crc_ok ? "OK" : "Failed"], ["Link quality (lower is better)", String(p.lqi)]]) {
      const row = el("div", "field");
      row.append(el("span", null, label), el("span", null, value));
      list.append(row);
    }
  }
  panel.append(list);

  const light = (index, on) => {
    if (index < 0) return;
    rows[index].classList.toggle("lit", on);
    cells.forEach((cell, i) => { if (owner[i] === index) cell.classList.toggle("lit", on); });
  };
  rows.forEach((row, index) => {
    row.addEventListener("mouseenter", () => light(index, true));
    row.addEventListener("mouseleave", () => light(index, false));
  });
  cells.forEach((cell, i) => {
    cell.addEventListener("mouseenter", () => light(owner[i], true));
    cell.addEventListener("mouseleave", () => light(owner[i], false));
  });
}

// --- Render loop -----------------------------------------------------------

let drawRequested = false;
let listsDirty = false;
let liveDirty = false;

function requestDraw() {
  if (drawRequested) return;
  drawRequested = true;
  requestAnimationFrame(() => {
    drawRequested = false;
    const device = currentDevice();
    for (const view of chartViews) drawChart(view, device);
    drawTiltDial(device);
    drawTimeline(device);
    drawTrack(device);
  });
}

function renderPanels() {
  const device = currentDevice();
  renderHeader(device);
  renderStepper(device);
  renderTiles(device);
  renderGps(device);
  renderPyro(device);
}

function refresh() {
  liveDirty = false;
  renderPanels();
  requestDraw();
  renderStream();
  if (selectedSeq === null) renderInspector();
}

function buildRanges() {
  const container = $("ranges");
  const buttons = RANGES.map(([label, seconds]) => {
    const button = el("button", null, label);
    button.type = "button";
    button.addEventListener("click", () => {
      rangeSeconds = seconds;
      buttons.forEach((b, i) => b.setAttribute("aria-pressed", String(RANGES[i][1] === seconds)));
      requestDraw();
    });
    button.setAttribute("aria-pressed", String(seconds === rangeSeconds));
    return button;
  });
  container.replaceChildren(...buttons);
}

function storedTheme() {
  try { return localStorage.getItem("theme"); } catch { return null; }
}

function setTheme(theme) {
  if (theme) document.documentElement.dataset.theme = theme;
  try { if (theme) localStorage.setItem("theme", theme); } catch { /* preference only */ }
  readColors();
  requestDraw();
}

function toggleTheme() {
  setTheme(getComputedStyle(document.documentElement).colorScheme.includes("dark") ? "light" : "dark");
}

function connect() {
  const events = new EventSource("/events");
  events.addEventListener("snapshot", (event) => {
    const snapshot = JSON.parse(event.data);
    devices.clear();
    feed = [];
    snapshot.packets.forEach(ingest);
    link = snapshot.status;
    serverLost = false;
    refresh();
    renderLink();
  });
  events.addEventListener("packet", (event) => {
    ingest(JSON.parse(event.data));
    if (feed.length > MAX_FEED) feed = feed.slice(-MAX_FEED);
    liveDirty = true;
  });
  events.addEventListener("status", (event) => {
    link = JSON.parse(event.data);
    renderLink();
  });
  events.addEventListener("error", () => {
    serverLost = true;
    renderLink();
  });
}

async function main() {
  schema = await (await fetch("/schema")).json();
  setTheme(storedTheme());
  buildRanges();
  buildCharts();
  $("theme").addEventListener("click", toggleTheme);
  $("device-select").addEventListener("change", (event) => {
    selectedSerial = Number(event.target.value);
    refresh();
  });
  $("timeline").addEventListener("pointermove", onTimelineHover);
  $("timeline").addEventListener("pointerleave", clearHover);
  new ResizeObserver(requestDraw).observe(document.querySelector("main"));
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => setTheme(null));
  refresh();
  connect();
  setInterval(() => { if (liveDirty) refresh(); }, 250);
  setInterval(() => {
    renderLink();
    renderTiles(currentDevice());  // keeps packets/s falling after the link goes quiet
  }, 500);
}

main();
