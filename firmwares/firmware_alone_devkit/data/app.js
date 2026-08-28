const DEFAULT_PINS = [13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];

const POLL_MS = 1000;
const FETCH_TIMEOUT_MS = 4000;
const WIFI_SCAN_POLL_MS = 800;
const WIFI_SCAN_MAX_POLLS = 20;
const STALE_AFTER_MS = 3000;
const GPIO_TAP_PULSE_MS = 250;
const GPIO_TAP_COOLDOWN_MS = 6000;
const CHART_MAX_SEC = 1800;
const TRENDS_FETCH_TIMEOUT_MS = 5000;
const TRENDS_PAGE = 240;
const TRENDS_HYDRATE_GAP_MS = 20000;
const HR_VIEW_BELT_KEY = 'oxypulse.hrView.belt';
const HR_VIEW_WELLUE_KEY = 'oxypulse.hrView.wellue';
const CHART_PAD = { top: 8, right: 8, bottom: 22, left: 42 };

const hostInput = document.getElementById('host');
const connStatus = document.getElementById('connStatus');
const uptimeEl = document.getElementById('uptime');
const linkBar = document.getElementById('linkBar');
const linkText = document.getElementById('linkText');
const linkMeta = document.getElementById('linkMeta');
const gpioPin = document.getElementById('gpioPin');
const servoPin = document.getElementById('servoPin');
const gpioState = document.getElementById('gpioState');
const gpioStateLabel = document.getElementById('gpioStateLabel');
const servoAngle = document.getElementById('servoAngle');
const angleValue = document.getElementById('angleValue');
const servoAuto = document.getElementById('servoAuto');
const servoStatus = document.getElementById('servoStatus');
const btnGpioApply = document.getElementById('btnGpioApply');
const btnGpioTap = document.getElementById('btnGpioTap');
const btnServoApply = document.getElementById('btnServoApply');
const btnFlowReset = document.getElementById('btnFlowReset');
const trendsHint = document.getElementById('trendsHint');
const chartO2 = document.getElementById('chartO2');
const chartWorkO2 = document.getElementById('chartWorkO2');
const chartFlow = document.getElementById('chartFlow');
const chartPressure = document.getElementById('chartPressure');
const chartWorkPressure = document.getElementById('chartWorkPressure');
const chartSpo2 = document.getElementById('chartSpo2');
const chartHr = document.getElementById('chartHr');
const chartCo2 = document.getElementById('chartCo2');
const chartRr = document.getElementById('chartRr');
const chartO2Now = document.getElementById('chartO2Now');
const chartWorkO2Now = document.getElementById('chartWorkO2Now');
const chartFlowNow = document.getElementById('chartFlowNow');
const chartPressureNow = document.getElementById('chartPressureNow');
const chartWorkPressureNow = document.getElementById('chartWorkPressureNow');
const chartSpo2Now = document.getElementById('chartSpo2Now');
const chartHrNow = document.getElementById('chartHrNow');
const chartCo2Now = document.getElementById('chartCo2Now');
const chartRrNow = document.getElementById('chartRrNow');
const hrBeltTitle = document.getElementById('hrBeltTitle');
const hrLegendBelt = document.getElementById('hrLegendBelt');
const hrLegendWellue = document.getElementById('hrLegendWellue');
const hrLegendBeltWrap = document.getElementById('hrLegendBeltWrap');
const hrLegendWellueWrap = document.getElementById('hrLegendWellueWrap');
const hrViewBeltInput = document.getElementById('hrViewBelt');
const hrViewWellueInput = document.getElementById('hrViewWellue');
const btnWifiSettings = document.getElementById('btnWifiSettings');
const wifiModal = document.getElementById('wifiModal');
const wifiModalBackdrop = document.getElementById('wifiModalBackdrop');
const btnWifiClose = document.getElementById('btnWifiClose');
const wifiModalStatus = document.getElementById('wifiModalStatus');
const btnWifiScan = document.getElementById('btnWifiScan');
const wifiScanStatus = document.getElementById('wifiScanStatus');
const wifiNetworkList = document.getElementById('wifiNetworkList');
const wifiSsidManual = document.getElementById('wifiSsidManual');
const wifiPassword = document.getElementById('wifiPassword');
const btnWifiSave = document.getElementById('btnWifiSave');
const btnWifiForget = document.getElementById('btnWifiForget');
const wifiLanHint = document.getElementById('wifiLanHint');
const btnHrvMeasure = document.getElementById('btnHrvMeasure');

const formControls = [gpioPin, servoPin, gpioState, servoAngle, servoAuto];

let baseUrl = '';
let pollTimer = null;
let linkUiTimer = null;
let lastStatus = null;
let lastPinsKey = '';
let lastOkAt = null;
let lastUptimeMs = null;
let lastError = '';
let lastPollFailed = true;
let pollInFlight = false;
let statusPollPaused = false;
let hydrateInFlight = false;
let hydrateOnNextOk = true;
let linkState = 'idle';
let pollingActive = false;
let formLocked = false;
let gpioSyncLocked = false;
let chartWindowSec = 300;
let selectedWifi = null;
let beltDisplayName = 'Нагрудный HR';
const emptySources = {
  o2: false,
  flow: false,
  pressure: false,
  co2: false,
  wellue: false,
  hr: false,
  rr: false,
  work_o2: false,
  work_pressure: false,
};
let trendSources = { ...emptySources };
const history = {
  t: [],
  espMs: [],
  spo2: [],
  hrWellue: [],
  hrCoospo: [],
  co2: [],
  o2: [],
  flow: [],
  pressure: [],
  workO2: [],
  workPressure: [],
  rr: [],
};

function anyTrendSource() {
  return Object.values(trendSources).some(Boolean);
}

function parseSources(raw) {
  const s = raw && typeof raw === 'object' ? raw : {};
  return {
    o2: !!s.o2,
    flow: !!s.flow,
    pressure: !!s.pressure,
    co2: !!s.co2,
    wellue: !!s.wellue,
    hr: !!s.hr,
    rr: !!s.rr,
    work_o2: !!s.work_o2,
    work_pressure: !!s.work_pressure,
  };
}

function loadHrViewFlag(key, fallback) {
  try {
    const v = localStorage.getItem(key);
    if (v === null) return fallback;
    return v === '1' || v === 'true';
  } catch (e) {
    return fallback;
  }
}

function saveHrViewFlag(key, value) {
  try {
    localStorage.setItem(key, value ? '1' : '0');
  } catch (e) {
    /* ignore quota / private mode */
  }
}

let hrViewBelt = loadHrViewFlag(HR_VIEW_BELT_KEY, true);
let hrViewWellue = loadHrViewFlag(HR_VIEW_WELLUE_KEY, true);

function syncHrViewInputs() {
  if (hrViewBeltInput) hrViewBeltInput.checked = hrViewBelt;
  if (hrViewWellueInput) hrViewWellueInput.checked = hrViewWellue;
}

function syncTrendButtons() {
  document.querySelectorAll('.btn-trend').forEach((btn) => {
    const key = btn.dataset.src;
    btn.classList.toggle('active', !!trendSources[key]);
  });
}

function setPanelHidden(id, hidden) {
  const el = document.getElementById(id);
  if (el) el.hidden = hidden;
}

function setGpioUiState(on, { pulsing = false } = {}) {
  gpioState.checked = on;
  gpioStateLabel.classList.toggle('pulse', pulsing);
  if (pulsing) {
    gpioStateLabel.textContent = 'ON · импульс';
  } else {
    gpioStateLabel.textContent = on ? 'ON' : 'OFF';
  }
}

function fillPinSelects(pins, preserveSelection = true) {
  for (const sel of [gpioPin, servoPin]) {
    const prev = preserveSelection ? sel.value : '';
    sel.innerHTML = '';
    for (const p of pins) {
      const opt = document.createElement('option');
      opt.value = String(p);
      opt.textContent = `GPIO ${p}`;
      sel.appendChild(opt);
    }
    if (prev && [...sel.options].some((o) => o.value === prev)) {
      sel.value = prev;
    }
  }
}

fillPinSelects(DEFAULT_PINS, false);

servoAngle.addEventListener('input', () => {
  angleValue.textContent = servoAngle.value;
});

gpioState.addEventListener('change', () => {
  gpioStateLabel.textContent = gpioState.checked ? 'ON' : 'OFF';
});

for (const el of formControls) {
  el.addEventListener('focus', () => {
    formLocked = true;
  });
  el.addEventListener('blur', () => {
    setTimeout(() => {
      if (!formControls.some((c) => c === document.activeElement)) {
        formLocked = false;
      }
    }, 150);
  });
}

gpioPin.addEventListener('change', () => {
  if (lastStatus) syncGpioForm(lastStatus);
});

servoPin.addEventListener('change', () => {
  if (lastStatus) syncServoForm(lastStatus);
});

function apiUrl(path) {
  return `${baseUrl}${path}`;
}

async function fetchWithTimeout(url, options = {}, timeoutMs = FETCH_TIMEOUT_MS) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, { ...options, signal: controller.signal });
    return res;
  } catch (e) {
    if (e.name === 'AbortError') {
      throw new Error('таймаут');
    }
    throw e;
  } finally {
    clearTimeout(timer);
  }
}

async function fetchStatus() {
  const res = await fetchWithTimeout(apiUrl('/api/status'));
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function sendCmd(body) {
  const res = await fetchWithTimeout(apiUrl('/api/cmd'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
}

function formatAge(ms) {
  if (ms < 1000) return 'только что';
  const sec = Math.floor(ms / 1000);
  if (sec < 60) return `${sec} с назад`;
  const min = Math.floor(sec / 60);
  return `${min} мин назад`;
}

function formatClock(ts) {
  return new Date(ts).toLocaleTimeString('ru-RU', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

function hostLabel() {
  if (!baseUrl) return '';
  return baseUrl.replace(/^https?:\/\//, '');
}

function computeLinkState() {
  if (!pollingActive || !baseUrl) return 'idle';
  if (!navigator.onLine) return 'offline';
  if (lastOkAt === null) return 'offline';
  const age = Date.now() - lastOkAt;
  if (age > STALE_AFTER_MS) return 'offline';
  if (age > POLL_MS * 2) return 'stale';
  return 'live';
}

function updateCommandButtons() {
  const disableCmds = linkState === 'offline' || !pollingActive;
  btnGpioApply.disabled = disableCmds;
  btnServoApply.disabled = disableCmds;
  btnFlowReset.disabled = disableCmds;
  if (!gpioSyncLocked) {
    btnGpioTap.disabled = disableCmds;
  }
}

function updateUptimeLine() {
  if (linkState === 'live' && lastUptimeMs !== null) {
    uptimeEl.textContent = `Uptime ESP32: ${(lastUptimeMs / 1000).toFixed(1)} s`;
    return;
  }
  if (lastOkAt !== null) {
    const parts = [`Последнее обновление: ${formatClock(lastOkAt)} (${formatAge(Date.now() - lastOkAt)})`];
    if (lastUptimeMs !== null) {
      parts.push(`Uptime было: ${(lastUptimeMs / 1000).toFixed(1)} s`);
    }
    uptimeEl.textContent = parts.join(' · ');
    return;
  }
  uptimeEl.textContent = '';
}

function renderLinkBar() {
  linkState = computeLinkState();
  linkBar.className = `link-bar ${linkState}`;

  const host = hostLabel();
  switch (linkState) {
    case 'live':
      linkText.textContent = 'Онлайн';
      linkMeta.textContent = host
        ? `${host} · обновлено ${formatAge(Date.now() - lastOkAt)}`
        : `обновлено ${formatAge(Date.now() - lastOkAt)}`;
      break;
    case 'stale':
      linkText.textContent = 'Задержка';
      linkMeta.textContent = `${formatAge(Date.now() - lastOkAt)} без ответа`;
      break;
    case 'offline':
      linkText.textContent = 'Нет связи';
      linkMeta.textContent = lastOkAt
        ? `данные устарели · ${formatAge(Date.now() - lastOkAt)}`
        : (lastError || 'ESP32 недоступен');
      break;
    default:
      linkText.textContent = 'Не подключено';
      linkMeta.textContent = host || '';
      break;
  }

  if (linkState === 'live') {
    connStatus.textContent = 'Подключено';
    connStatus.className = 'status ok';
  } else if (linkState === 'stale') {
    connStatus.textContent = 'Задержка ответа';
    connStatus.className = 'status warn';
  } else if (linkState === 'offline') {
    connStatus.textContent = lastError ? `Ошибка: ${lastError}` : 'Нет связи';
    connStatus.className = 'status err';
  } else {
    connStatus.textContent = 'Не подключено';
    connStatus.className = 'status';
  }

  updateUptimeLine();
  updateCommandButtons();
}

function startLinkUiTimer() {
  if (linkUiTimer) clearInterval(linkUiTimer);
  linkUiTimer = setInterval(renderLinkBar, 250);
}

function initPinSelects(data) {
  const pins = Array.isArray(data.whitelist) && data.whitelist.length
    ? data.whitelist
    : DEFAULT_PINS;
  const pinsKey = pins.join(',');
  if (pinsKey === lastPinsKey) return;
  fillPinSelects(pins);
  lastPinsKey = pinsKey;
}

function renderFlowPhase(phase) {
  const el = document.getElementById('flowPhase');
  if (!el) return;
  const labels = { inhale: 'Вдох', exhale: 'Выдох', idle: 'Покой' };
  const key = phase && labels[phase] ? phase : 'idle';
  el.textContent = labels[key];
  el.className = `flow-phase ${key}`;
}

function renderSensorTile(id, ok, text) {
  const tile = document.getElementById(id);
  if (!tile) return;
  tile.classList.toggle('error', !ok);
  const valueEl = tile.querySelector('.sensor-value');
  if (valueEl) valueEl.textContent = text;
}

function syncGpioForm(data) {
  if (gpioSyncLocked || data.gpio_tap_active) {
    return;
  }
  const gp = Number(gpioPin.value);
  const gpioItem = (data.gpio || []).find((g) => g.pin === gp);
  if (gpioItem) {
    setGpioUiState(gpioItem.state === 1);
  }
}

function syncServoForm(data) {
  const sp = Number(servoPin.value);
  const servoItem = (data.servos || []).find((s) => s.pin === sp);
  if (servoItem) {
    servoAngle.value = servoItem.angle;
    angleValue.textContent = String(servoItem.angle);
    servoAuto.checked = !!servoItem.auto;
  }
}

function renderServoStatus(data) {
  const sp = Number(servoPin.value);
  const servoItem = (data.servos || []).find((s) => s.pin === sp);
  if (servoItem) {
    servoStatus.textContent = `На устройстве: ${servoItem.angle}°, auto=${servoItem.auto}`;
  } else {
    servoStatus.textContent = 'Серва на этом pin ещё не настроена';
  }
}

function finiteOrNull(ok, value, extraOk = true) {
  if (!ok || !extraOk) return null;
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function lastFinite(arr) {
  for (let i = arr.length - 1; i >= 0; i -= 1) {
    if (arr[i] != null && Number.isFinite(arr[i])) return arr[i];
  }
  return null;
}

function missingToNull(value) {
  const n = Number(value);
  if (!Number.isFinite(n) || n <= -30000) return null;
  return n;
}

function fromScaled10(value) {
  const n = missingToNull(value);
  return n == null ? null : n / 10;
}

function saneSpo2(value) {
  const n = missingToNull(value);
  if (n == null || n < 50 || n > 100) return null;
  return n;
}

function saneHr(value) {
  const n = missingToNull(value);
  if (n == null || n < 25 || n > 240) return null;
  return n;
}

function saneRr(value) {
  const n = missingToNull(value);
  if (n == null || n < 250 || n > 2000) return null;
  return n;
}

function trimHistory() {
  while (history.t.length > CHART_MAX_SEC) {
    history.t.shift();
    history.espMs.shift();
    history.spo2.shift();
    history.hrWellue.shift();
    history.hrCoospo.shift();
    history.co2.shift();
    history.o2.shift();
    history.flow.shift();
    history.pressure.shift();
    history.workO2.shift();
    history.workPressure.shift();
    history.rr.shift();
  }
}

function clearHistory() {
  history.t = [];
  history.espMs = [];
  history.spo2 = [];
  history.hrWellue = [];
  history.hrCoospo = [];
  history.co2 = [];
  history.o2 = [];
  history.flow = [];
  history.pressure = [];
  history.workO2 = [];
  history.workPressure = [];
  history.rr = [];
}

function applyTrendsPayload(data, { replace = true } = {}) {
  const count = Number(data.count || 0);
  const interval = Number(data.interval_ms || 1000);
  const nowMs = Number(data.now_ms || 0);
  const t0 = Number(data.t0_ms || 0);
  const offset = Number(data.offset || 0);
  const wallNow = Date.now();
  const spo2 = Array.isArray(data.spo2) ? data.spo2 : [];
  const hrW = Array.isArray(data.hr_wellue) ? data.hr_wellue : [];
  const hrC = Array.isArray(data.hr_coospo) ? data.hr_coospo : [];
  const co2 = Array.isArray(data.co2) ? data.co2 : [];
  const o2 = Array.isArray(data.o2) ? data.o2 : [];
  const flow = Array.isArray(data.flow) ? data.flow : [];
  const pressure = Array.isArray(data.pressure) ? data.pressure : [];
  const workO2 = Array.isArray(data.work_o2) ? data.work_o2 : [];
  const workPressure = Array.isArray(data.work_pressure) ? data.work_pressure : [];
  const rr = Array.isArray(data.rr) ? data.rr : [];
  if (data.sources) {
    trendSources = parseSources(data.sources);
    syncTrendButtons();
  }

  if (replace || offset === 0) {
    clearHistory();
  }

  for (let i = 0; i < count; i += 1) {
    const sampleMs = t0 + (offset + i) * interval;
    history.espMs.push(sampleMs);
    history.t.push(wallNow - (nowMs - sampleMs));
    history.spo2.push(saneSpo2(spo2[i]));
    history.hrWellue.push(saneHr(hrW[i]));
    history.hrCoospo.push(saneHr(hrC[i]));
    history.co2.push(missingToNull(co2[i]));
    history.o2.push(fromScaled10(o2[i]));
    history.flow.push(fromScaled10(flow[i]));
    history.pressure.push(fromScaled10(pressure[i]));
    history.workO2.push(fromScaled10(workO2[i]));
    history.workPressure.push(fromScaled10(workPressure[i]));
    history.rr.push(saneRr(rr[i]));
  }
  trimHistory();
}

function shouldHydrateAfterGap(data) {
  if (!anyTrendSource()) return false;
  if (!history.t.length) return true;
  const espMs = Number(data.uptime_ms);
  const last = lastHistoryEspMs();
  if (!Number.isFinite(espMs) || last == null) return true;
  if (espMs + 2000 < last) return true;
  return (espMs - last) > TRENDS_HYDRATE_GAP_MS;
}

async function hydrateTrends() {
  if (!baseUrl || hydrateInFlight) return;
  hydrateInFlight = true;
  try {
    let offset = 0;
    let replace = true;
    for (let page = 0; page < 16; page += 1) {
      const res = await fetchWithTimeout(
        apiUrl(`/api/trends?offset=${offset}&limit=${TRENDS_PAGE}`),
        {},
        TRENDS_FETCH_TIMEOUT_MS,
      );
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      applyTrendsPayload(data, { replace });
      replace = false;
      const total = Number(data.total || 0);
      const count = Number(data.count || 0);
      offset += count;
      if (count === 0 || offset >= total) break;
    }
    updateTrendsHint();
    drawAllCharts();
  } catch (e) {
    if (trendsHint) {
      trendsHint.textContent = `История ESP32 недоступна: ${e.message}`;
    }
  } finally {
    hydrateInFlight = false;
  }
}

function lastHistoryEspMs() {
  if (!history.espMs.length) return null;
  return history.espMs[history.espMs.length - 1];
}

function pushHistory(data) {
  const s = data.sensors || {};
  const wellue = s.wellue || {};
  const coospo = s.coospo || {};
  const co2 = (s.working && s.working.co2) || s.co2 || {};
  const o2 = (s.hypoxia && s.hypoxia.o2) || s.o2 || {};
  const flow = (s.working && s.working.flow) || s.flow || {};
  const pressure = (s.hypoxia && s.hypoxia.pressure) || s.pressure || {};
  const workO2 = (s.working && s.working.o2) || {};
  const workPressure = (s.working && s.working.pressure) || {};
  const wellueLive = !!trendSources.wellue && !!wellue.ok && !!wellue.contact;
  const coospoLive = !!trendSources.hr && !!coospo.ok && (coospo.contact !== false);
  const rrLive = !!trendSources.rr && !!coospo.ok && (coospo.contact !== false);
  const espMs = Number(data.uptime_ms);
  const lastEsp = lastHistoryEspMs();
  if (Number.isFinite(espMs) && lastEsp != null && (espMs - lastEsp) < 500) {
    return;
  }

  const wall = Number.isFinite(espMs)
    ? Date.now() - Math.max(0, (Number(data.uptime_ms) || 0) - espMs)
    : Date.now();
  history.t.push(wall);
  history.espMs.push(Number.isFinite(espMs) ? espMs : null);
  history.spo2.push(wellueLive ? saneSpo2(wellue.spo2) : null);
  history.hrWellue.push(wellueLive ? saneHr(wellue.hr) : null);
  history.hrCoospo.push(coospoLive ? saneHr(coospo.bpm) : null);
  history.co2.push(trendSources.co2 ? finiteOrNull(!!co2.ok, co2.ppm) : null);
  history.o2.push(trendSources.o2 ? finiteOrNull(!!o2.ok, o2.percent) : null);
  history.flow.push(trendSources.flow ? finiteOrNull(!!flow.ok, flow.ve_lpm) : null);
  history.pressure.push(trendSources.pressure ? finiteOrNull(!!pressure.ok, pressure.hpa) : null);
  history.workO2.push(trendSources.work_o2 ? finiteOrNull(!!workO2.ok, workO2.percent) : null);
  history.workPressure.push(
    trendSources.work_pressure ? finiteOrNull(!!workPressure.ok, workPressure.hpa) : null,
  );
  history.rr.push(rrLive ? saneRr(coospo.rr_ms) : null);
  trimHistory();
}

function pointsInWindow(values) {
  const now = Date.now();
  const start = now - chartWindowSec * 1000;
  const pts = [];
  for (let i = 0; i < history.t.length; i += 1) {
    if (history.t[i] >= start) {
      pts.push({ t: history.t[i], v: values[i] });
    }
  }
  return pts;
}

function valuesOf(pts) {
  return pts.map((p) => p.v).filter((v) => v != null && Number.isFinite(v));
}

function niceRange(vals, fallbackMin, fallbackMax) {
  if (!vals.length) return { min: fallbackMin, max: fallbackMax };
  let min = Math.min(...vals);
  let max = Math.max(...vals);
  if (min === max) {
    const span = Math.max(1, (fallbackMax - fallbackMin) * 0.08);
    min -= span;
    max += span;
  }
  const pad = (max - min) * 0.12;
  return { min: min - pad, max: max + pad };
}

function formatRange(vals, format) {
  if (!vals.length) return '';
  const min = Math.min(...vals);
  const max = Math.max(...vals);
  const last = vals[vals.length - 1];
  if (min === max) return format(last);
  return `${format(last)}  ·  ${format(min)}–${format(max)}`;
}

function xOf(t, now, x0, plotW) {
  const start = now - chartWindowSec * 1000;
  const span = Math.max(1, now - start);
  return x0 + ((t - start) / span) * plotW;
}

function yOf(v, min, max, y0, plotH) {
  const span = max - min || 1;
  return y0 + plotH - ((v - min) / span) * plotH;
}

function setupCanvas(canvas) {
  const dpr = window.devicePixelRatio || 1;
  const cssW = canvas.clientWidth || canvas.parentElement.clientWidth || 320;
  const cssH = canvas.clientHeight || 148;
  const w = Math.max(1, Math.round(cssW * dpr));
  const h = Math.max(1, Math.round(cssH * dpr));
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { ctx, w: cssW, h: cssH };
}

window.__oxy = {
  getApiUrl: (path) => apiUrl(path),
  fetchWithTimeout,
  setupCanvas,
  getBaseUrl: () => baseUrl,
  setStatusPollPaused(paused) {
    statusPollPaused = !!paused;
  },
};

function loadHrvModule() {
  if (!window.OxyHrv) {
    return Promise.reject(
      new Error('hrv.js не найден на ESP32. Выполните: cd firmware && pio run -t uploadfs'),
    );
  }
  window.OxyHrv.init();
  return Promise.resolve();
}

function drawSeries(ctx, pts, now, x0, y0, plotW, plotH, yMin, yMax, color, dashed) {
  ctx.save();
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.6;
  ctx.lineJoin = 'round';
  ctx.lineCap = 'round';
  if (dashed) ctx.setLineDash([5, 4]);
  let open = false;
  let prevT = null;
  ctx.beginPath();
  for (const p of pts) {
    if (p.v == null || !Number.isFinite(p.v)) {
      open = false;
      prevT = null;
      continue;
    }
    const x = xOf(p.t, now, x0, plotW);
    const y = yOf(p.v, yMin, yMax, y0, plotH);
    if (!open || (prevT != null && p.t - prevT > POLL_MS * 2.5)) {
      ctx.moveTo(x, y);
      open = true;
    } else {
      ctx.lineTo(x, y);
    }
    prevT = p.t;
  }
  ctx.stroke();
  ctx.restore();
}

function drawTrendChart(canvas, series, yFallback, emptyText) {
  if (!canvas) return;
  const { ctx, w, h } = setupCanvas(canvas);
  ctx.clearRect(0, 0, w, h);

  const now = Date.now();
  const x0 = CHART_PAD.left;
  const y0 = CHART_PAD.top;
  const plotW = Math.max(1, w - CHART_PAD.left - CHART_PAD.right);
  const plotH = Math.max(1, h - CHART_PAD.top - CHART_PAD.bottom);
  const allVals = series.flatMap((s) => valuesOf(s.pts));
  const { min: yMin, max: yMax } = niceRange(allVals, yFallback.min, yFallback.max);

  ctx.fillStyle = '#101010';
  ctx.fillRect(x0, y0, plotW, plotH);

  const ticks = 4;
  ctx.font = '10px system-ui, sans-serif';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  for (let i = 0; i <= ticks; i += 1) {
    const frac = i / ticks;
    const y = y0 + plotH * (1 - frac);
    const v = yMin + (yMax - yMin) * frac;
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x0, y);
    ctx.lineTo(x0 + plotW, y);
    ctx.stroke();
    ctx.fillStyle = '#777';
    ctx.fillText(yFallback.format(v), x0 - 6, y);
  }

  ctx.strokeStyle = '#333';
  ctx.strokeRect(x0 + 0.5, y0 + 0.5, plotW - 1, plotH - 1);

  const hasLine = series.some((s) => valuesOf(s.pts).length >= 1);
  if (!hasLine) {
    ctx.fillStyle = '#666';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.font = '12px system-ui, sans-serif';
    ctx.fillText(emptyText, x0 + plotW / 2, y0 + plotH / 2);
  } else {
    for (const s of series) {
      drawSeries(ctx, s.pts, now, x0, y0, plotW, plotH, yMin, yMax, s.color, !!s.dashed);
    }
  }

  ctx.fillStyle = '#666';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.font = '10px system-ui, sans-serif';
  ctx.fillText(`−${chartWindowSec / 60} мин`, x0, y0 + plotH + 6);
  ctx.textAlign = 'right';
  ctx.fillText('сейчас', x0 + plotW, y0 + plotH + 6);
}

function updateTrendsHint() {
  if (!trendsHint) return;
  if (!anyTrendSource()) {
    trendsHint.textContent = 'Включите «Тренд» на плитке датчика — набор хранится на ESP32';
    return;
  }
  const min = chartWindowSec / 60;
  const filled = history.t.length;
  const filledMin = Math.max(1, Math.round(filled / 60));
  const span = filled < 60 ? `${filled} с` : `${filledMin} мин`;
  trendsHint.textContent =
    `Окно ${min} мин · 1 Гц · история на ESP32 (${span} из ~30 мин, RAM)`;
}

function drawPanel(panelId, canvas, nowEl, pts, color, fallback, formatNow, emptyText) {
  setPanelHidden(panelId, false);
  const vals = valuesOf(pts);
  if (nowEl) nowEl.textContent = formatRange(vals, formatNow) || '—';
  drawTrendChart(canvas, [{ pts, color }], fallback, emptyText);
}

function drawAllCharts() {
  const showO2 = !!trendSources.o2;
  const showWorkO2 = !!trendSources.work_o2;
  const showFlow = !!trendSources.flow;
  const showPressure = !!trendSources.pressure;
  const showWorkPressure = !!trendSources.work_pressure;
  const showCo2 = !!trendSources.co2;
  const showWellue = !!trendSources.wellue;
  const showHrBelt = !!trendSources.hr;
  const showRr = !!trendSources.rr;
  const showHr = showWellue || showHrBelt;
  const dualHr = showWellue && showHrBelt;
  const drawBeltHr = showHrBelt && (!dualHr || hrViewBelt);
  const drawWellueHr = showWellue && (!dualHr || hrViewWellue);

  setPanelHidden('panelO2', !showO2);
  setPanelHidden('panelWorkO2', !showWorkO2);
  setPanelHidden('panelFlow', !showFlow);
  setPanelHidden('panelPressure', !showPressure);
  setPanelHidden('panelWorkPressure', !showWorkPressure);
  setPanelHidden('panelCo2', !showCo2);
  setPanelHidden('panelSpo2', !showWellue);
  setPanelHidden('panelHr', !showHr);
  setPanelHidden('panelRr', !showRr);

  if (showO2) {
    drawPanel('panelO2', chartO2, chartO2Now, pointsInWindow(history.o2), '#22d3ee',
      { min: 18, max: 22, format: (v) => v.toFixed(1) },
      (v) => `${v.toFixed(1)} %`, 'нет O₂');
  }
  if (showWorkO2) {
    drawPanel('panelWorkO2', chartWorkO2, chartWorkO2Now, pointsInWindow(history.workO2), '#67e8f9',
      { min: 18, max: 22, format: (v) => v.toFixed(1) },
      (v) => `${v.toFixed(1)} %`, 'нет O₂');
  }
  if (showFlow) {
    drawPanel('panelFlow', chartFlow, chartFlowNow, pointsInWindow(history.flow), '#a78bfa',
      { min: 0, max: 30, format: (v) => v.toFixed(1) },
      (v) => `${v.toFixed(1)} л/мин`, 'нет данных');
  }
  if (showPressure) {
    drawPanel('panelPressure', chartPressure, chartPressureNow, pointsInWindow(history.pressure), '#e879f9',
      { min: 980, max: 1040, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(1)} hPa`, 'нет давления');
  }
  if (showWorkPressure) {
    drawPanel('panelWorkPressure', chartWorkPressure, chartWorkPressureNow,
      pointsInWindow(history.workPressure), '#f0abfc',
      { min: 980, max: 1040, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(1)} hPa`, 'нет давления');
  }
  if (showCo2) {
    drawPanel('panelCo2', chartCo2, chartCo2Now, pointsInWindow(history.co2), '#60a5fa',
      { min: 400, max: 2000, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(0)} ppm`, 'нет CO₂');
  }
  if (showWellue) {
    drawPanel('panelSpo2', chartSpo2, chartSpo2Now, pointsInWindow(history.spo2), '#4ade80',
      { min: 90, max: 100, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(0)} %`, 'нет SpO₂');
  }

  if (hrLegendBelt) hrLegendBelt.textContent = beltDisplayName;
  if (hrLegendBeltWrap) {
    hrLegendBeltWrap.hidden = !showHrBelt;
    hrLegendBeltWrap.classList.toggle('plain', !dualHr);
  }
  if (hrLegendWellueWrap) {
    hrLegendWellueWrap.hidden = !showWellue;
    hrLegendWellueWrap.classList.toggle('plain', !dualHr);
  }
  syncHrViewInputs();

  if (showHr) {
    const hrCoospoPts = drawBeltHr ? pointsInWindow(history.hrCoospo) : [];
    const hrWelluePts = drawWellueHr ? pointsInWindow(history.hrWellue) : [];
    const parts = [];
    if (drawBeltHr) {
      const last = lastFinite(hrCoospoPts.map((p) => p.v));
      if (last != null) parts.push(`${beltDisplayName} ${last.toFixed(0)}`);
    }
    if (drawWellueHr) {
      const last = lastFinite(hrWelluePts.map((p) => p.v));
      if (last != null) parts.push(`Wellue ${last.toFixed(0)}`);
    }
    if (chartHrNow) chartHrNow.textContent = parts.length ? `${parts.join(' · ')} bpm` : '—';
    const hrSeries = [];
    if (drawBeltHr) hrSeries.push({ pts: hrCoospoPts, color: '#f87171' });
    if (drawWellueHr) hrSeries.push({ pts: hrWelluePts, color: '#fb923c', dashed: true });
    drawTrendChart(
      chartHr,
      hrSeries,
      { min: 50, max: 140, format: (v) => v.toFixed(0) },
      'нет ЧСС',
    );
  }

  if (showRr) {
    drawPanel('panelRr', chartRr, chartRrNow, pointsInWindow(history.rr), '#f87171',
      { min: 400, max: 1200, format: (v) => v.toFixed(0) },
      (v) => `${v.toFixed(0)} мс`, 'нет R-R');
  }
}

function setChartWindow(sec) {
  chartWindowSec = sec;
  document.querySelectorAll('.window-pill').forEach((btn) => {
    btn.classList.toggle('active', Number(btn.dataset.window) === sec);
  });
  updateTrendsHint();
  drawAllCharts();
}

function renderWifiLanHint(wifi) {
  if (!wifiLanHint) return;
  const sta = wifi?.sta || {};
  const apIp = wifi?.ap_ip || '192.168.4.1';
  const host = hostLabel();
  const onAp = !host || host === apIp || host.startsWith('192.168.4.');

  if (sta.connected && sta.ip) {
    wifiLanHint.hidden = false;
    if (onAp) {
      wifiLanHint.innerHTML = `В LAN: <a href="http://${sta.ip}/" target="_blank" rel="noopener">${sta.ip}</a> (${sta.ssid || 'WiFi'})`;
    } else {
      wifiLanHint.textContent = `STA: ${sta.ssid || '—'} · ${sta.ip} · RSSI ${sta.rssi ?? '—'} dBm`;
    }
    return;
  }
  if (sta.configured && sta.error) {
    wifiLanHint.hidden = false;
    wifiLanHint.textContent = `STA «${sta.ssid || '—'}»: ${sta.error}`;
    return;
  }
  wifiLanHint.hidden = true;
  wifiLanHint.textContent = '';
}

function formatWifiModalStatus(wifi) {
  if (!wifi) return 'AP для настройки · STA не задана';
  const sta = wifi.sta || {};
  const parts = [`AP ${wifi.ap_ssid || 'ESP32-Tester'} → ${wifi.ap_ip || '192.168.4.1'}`];
  if (sta.configured) {
    if (sta.connected) {
      parts.push(`STA «${sta.ssid}» → ${sta.ip} (${sta.rssi} dBm)`);
    } else {
      parts.push(`STA «${sta.ssid}»: ${sta.error || 'не подключена'}`);
    }
  } else {
    parts.push('STA не настроена');
  }
  return parts.join(' · ');
}

function openWifiModal() {
  if (!wifiModal) return;
  wifiModal.hidden = false;
  wifiModal.setAttribute('aria-hidden', 'false');
  if (wifiModalStatus && lastStatus) {
    wifiModalStatus.textContent = formatWifiModalStatus(lastStatus.wifi);
  }
  updateWifiSaveButton();
  renderWifiSelection();
}

function closeWifiModal() {
  if (!wifiModal) return;
  wifiModal.hidden = true;
  wifiModal.setAttribute('aria-hidden', 'true');
}

function getWifiTargetSsid() {
  const manual = wifiSsidManual?.value.trim() || '';
  if (manual) return manual;
  return selectedWifi?.ssid || '';
}

function updateWifiSaveButton() {
  if (!btnWifiSave) return;
  btnWifiSave.disabled = !getWifiTargetSsid() || !baseUrl;
}

function renderWifiSelection() {
  if (!wifiNetworkList) return;
  wifiNetworkList.querySelectorAll('.wifi-item').forEach((btn) => {
    const ssid = btn.dataset.ssid || '';
    btn.classList.toggle('selected', selectedWifi && selectedWifi.ssid === ssid);
  });
  updateWifiSaveButton();
}

function selectWifiNetwork(net) {
  selectedWifi = net;
  if (wifiSsidManual) wifiSsidManual.value = net.ssid;
  updateWifiSaveButton();
}

function renderWifiNetworkList(networks) {
  if (!wifiNetworkList) return;
  wifiNetworkList.innerHTML = '';
  for (const net of networks) {
    const li = document.createElement('li');
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'wifi-item';
    btn.dataset.ssid = net.ssid;
    const lock = net.secure ? '<span class="wifi-lock">защищена</span>' : 'открытая';
    btn.innerHTML = `<div>${net.ssid}</div><div class="wifi-item-meta"><span>${lock}</span><span>${net.rssi} dBm · ch ${net.channel ?? '—'}</span></div>`;
    btn.addEventListener('click', () => selectWifiNetwork({ ssid: net.ssid, secure: !!net.secure }));
    li.appendChild(btn);
    wifiNetworkList.appendChild(li);
  }
  renderWifiSelection();
}

async function fetchWifiScanStatus() {
  const res = await fetchWithTimeout(apiUrl('/api/wifi/scan'), {}, FETCH_TIMEOUT_MS);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function scanWifiNetworks() {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  btnWifiScan.disabled = true;
  if (wifiScanStatus) {
    wifiScanStatus.textContent = 'Запуск скана… AP может кратко моргнуть';
  }

  try {
    const startRes = await fetchWithTimeout(apiUrl('/api/wifi/scan'), { method: 'POST' }, FETCH_TIMEOUT_MS);
    if (!startRes.ok) throw new Error(`HTTP ${startRes.status}`);

    for (let i = 0; i < WIFI_SCAN_MAX_POLLS; i += 1) {
      await sleep(WIFI_SCAN_POLL_MS);
      let data;
      try {
        data = await fetchWifiScanStatus();
      } catch (e) {
        if (wifiScanStatus) {
          wifiScanStatus.textContent = `Скан… (переподключитесь к AP при обрыве) · ${e.message}`;
        }
        continue;
      }

      if (data.scanning) {
        if (wifiScanStatus) wifiScanStatus.textContent = `Сканирование… ${i + 1}/${WIFI_SCAN_MAX_POLLS}`;
        continue;
      }

      const networks = Array.isArray(data.networks) ? data.networks : [];
      if (data.error) {
        if (wifiScanStatus) wifiScanStatus.textContent = `Ошибка: ${data.error}`;
      } else if (networks.length) {
        if (wifiScanStatus) wifiScanStatus.textContent = `Найдено: ${networks.length}`;
      } else {
        if (wifiScanStatus) wifiScanStatus.textContent = 'Сети не найдены — введите SSID вручную';
      }
      renderWifiNetworkList(networks);
      return;
    }
    if (wifiScanStatus) wifiScanStatus.textContent = 'Таймаут скана — введите SSID вручную';
  } catch (e) {
    if (wifiScanStatus) wifiScanStatus.textContent = `Ошибка: ${e.message}. Введите SSID вручную.`;
  } finally {
    btnWifiScan.disabled = false;
  }
}

async function postWifiConfig(body) {
  const res = await fetchWithTimeout(apiUrl('/api/wifi/config'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  }, 8000);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function saveWifiConfig() {
  const ssid = getWifiTargetSsid();
  if (!ssid) return;
  try {
    btnWifiSave.disabled = true;
    btnWifiSave.textContent = 'Сохранение…';
    await postWifiConfig({
      ssid,
      password: wifiPassword?.value || '',
    });
    if (wifiScanStatus) wifiScanStatus.textContent = 'Перезагрузка ESP32…';
    closeWifiModal();
    pollingActive = false;
    if (pollTimer) clearInterval(pollTimer);
    linkState = 'offline';
    renderLinkBar();
  } catch (e) {
    alert(`WiFi: ${e.message}`);
    updateWifiSaveButton();
    btnWifiSave.textContent = 'Сохранить и перезагрузить';
  }
}

async function forgetWifiConfig() {
  if (!confirm('Удалить сохранённую WiFi-сеть и перезагрузить ESP32?')) return;
  try {
    btnWifiForget.disabled = true;
    await postWifiConfig({ clear: true });
    closeWifiModal();
    pollingActive = false;
    if (pollTimer) clearInterval(pollTimer);
  } catch (e) {
    alert(`WiFi: ${e.message}`);
  } finally {
    btnWifiForget.disabled = false;
  }
}

function renderBleTile(id, data, formatOk) {
  const connected = !!data.connected;
  const ok = !!data.ok;
  const error = data.error ? String(data.error) : '';
  if (!connected) {
    const text = error ? `поиск…\n${error}` : 'поиск…';
    renderSensorTile(id, false, text);
    return;
  }
  if (!ok) {
    const text = error ? `подключено — нет данных\n${error}` : 'подключено — нет данных';
    renderSensorTile(id, false, text);
    return;
  }
  renderSensorTile(id, true, formatOk(data));
}

function renderTelemetry(data) {
  lastUptimeMs = data.uptime_ms;
  renderServoStatus(data);

  const s = data.sensors || {};
  const hypoxia = s.hypoxia || {};
  const working = s.working || {};
  const o2 = hypoxia.o2 || s.o2 || {};
  const flow = working.flow || s.flow || {};
  const pressure = hypoxia.pressure || s.pressure || {};
  const co2 = working.co2 || s.co2 || {};
  const workO2 = working.o2 || {};
  const workPressure = working.pressure || {};

  renderSensorTile(
    'sensorO2',
    !!o2.ok,
    o2.ok
      ? `O₂: ${Number(o2.percent || 0).toFixed(1)} % (${Number(o2.mv || 0).toFixed(2)} mV)`
      : 'нет данных',
  );
  renderSensorTile(
    'sensorWorkO2',
    !!workO2.ok,
    workO2.ok
      ? `O₂: ${Number(workO2.percent || 0).toFixed(1)} % (${Number(workO2.mv || 0).toFixed(2)} mV)`
      : 'нет данных',
  );
  renderFlowPhase(flow.phase);
  renderSensorTile(
    'sensorFlow',
    !!flow.ok,
    flow.ok
      ? `Поток: ${Number(flow.slm || 0).toFixed(2)} slm\nВдох (>0): ${Number(flow.inhale_l || 0).toFixed(3)} л\nВыдох (<0): ${Number(flow.exhale_l || 0).toFixed(3)} л\nСреднее (30 с): ${Number(flow.ve_lpm || 0).toFixed(1)} л/мин`
      : 'нет данных',
  );
  renderSensorTile(
    'sensorPressure',
    !!pressure.ok,
    pressure.ok
      ? `${Number(pressure.hpa || 0).toFixed(1)} hPa, ${Number(pressure.temp_c || 0).toFixed(1)} °C`
      : 'нет данных',
  );
  renderSensorTile(
    'sensorWorkPressure',
    !!workPressure.ok,
    workPressure.ok
      ? `${Number(workPressure.hpa || 0).toFixed(1)} hPa, ${Number(workPressure.temp_c || 0).toFixed(1)} °C`
      : 'нет данных',
  );
  const co2Warmup = co2.warming_up ? ' (прогрев)' : '';
  renderSensorTile(
    'sensorCo2',
    !!co2.ok,
    co2.ok
      ? `CO₂: ${Number(co2.ppm || 0)} ppm (${Number(co2.percent ?? (co2.ppm || 0) / 10000).toFixed(2)} %)${co2Warmup}\nT: ${Number(co2.temp_c || 0).toFixed(1)} °C, RH: ${Number(co2.rh || 0).toFixed(1)} %`
      : 'нет данных',
  );

  const wellue = s.wellue || {};
  const coospo = s.coospo || {};
  beltDisplayName = (coospo.name && String(coospo.name).trim()) || 'Нагрудный HR';
  if (hrBeltTitle) hrBeltTitle.textContent = beltDisplayName;
  renderBleTile('sensorWellue', wellue, (d) => (
    `${d.name || 'Wellue'}\nSpO₂: ${Number(d.spo2 || 0)} %\nЧСС: ${Number(d.hr || 0)} bpm\nБатарея: ${Number(d.battery || 0)} %\nКонтакт: ${d.contact ? 'да' : 'нет'}\nMotion: ${Number(d.motion || 0)}`
  ));
  renderBleTile('sensorCoospo', coospo, (d) => {
    const rr = Number(d.rr_ms || 0);
    const rrLine = rr > 0 ? `\nR-R: ${rr} мс` : '';
    return `${d.name || beltDisplayName}\nЧСС: ${Number(d.bpm || 0)} bpm${rrLine}\nКонтакт: ${d.contact ? 'да' : 'нет'}`;
  });
  if (btnHrvMeasure) {
    btnHrvMeasure.disabled = !coospo.connected;
  }
  if (anyTrendSource()) {
    pushHistory(data);
  }
  updateTrendsHint();
  drawAllCharts();
}

function applyStatus(data, { syncForm = false } = {}) {
  lastStatus = data;
  if (data.trends && data.trends.sources) {
    trendSources = parseSources(data.trends.sources);
    syncTrendButtons();
  }
  initPinSelects(data);
  renderTelemetry(data);
  renderWifiLanHint(data.wifi);
  if (wifiModal && !wifiModal.hidden && wifiModalStatus) {
    wifiModalStatus.textContent = formatWifiModalStatus(data.wifi);
  }
  if (syncForm && !formLocked) {
    syncGpioForm(data);
    syncServoForm(data);
  }
}

async function poll({ syncForm = false } = {}) {
  if (!baseUrl || pollInFlight || statusPollPaused) return;
  pollInFlight = true;
  try {
    const recovered = lastPollFailed;
    const data = await fetchStatus();
    lastPollFailed = false;
    lastOkAt = Date.now();
    lastError = '';
    const needHydrate = hydrateOnNextOk || (recovered && shouldHydrateAfterGap(data));
    if (needHydrate) {
      hydrateOnNextOk = false;
      await hydrateTrends();
    }
    applyStatus(data, { syncForm });
  } catch (e) {
    lastPollFailed = true;
    lastError = e.message;
  } finally {
    pollInFlight = false;
    renderLinkBar();
  }
}

function startPolling() {
  pollingActive = true;
  lastOkAt = null;
  lastError = '';
  lastPollFailed = true;
  hydrateOnNextOk = true;
  if (pollTimer) clearInterval(pollTimer);
  startLinkUiTimer();
  renderLinkBar();
  poll({ syncForm: true });
  pollTimer = setInterval(() => poll({ syncForm: false }), POLL_MS);
}

document.getElementById('btnConnect').addEventListener('click', () => {
  const host = hostInput.value.trim().replace(/\/$/, '');
  baseUrl = host.startsWith('http') ? host : `http://${host}`;
  lastPinsKey = '';
  updateWifiSaveButton();
  startPolling();
});

btnWifiSettings?.addEventListener('click', openWifiModal);
btnWifiClose?.addEventListener('click', closeWifiModal);
wifiModalBackdrop?.addEventListener('click', closeWifiModal);
wifiSsidManual?.addEventListener('input', () => {
  selectedWifi = null;
  renderWifiSelection();
});
btnWifiScan?.addEventListener('click', scanWifiNetworks);
btnWifiSave?.addEventListener('click', saveWifiConfig);
btnWifiForget?.addEventListener('click', forgetWifiConfig);

btnGpioApply.addEventListener('click', async () => {
  try {
    await sendCmd({
      cmd: 'gpio_set',
      pin: Number(gpioPin.value),
      state: gpioState.checked ? 1 : 0,
    });
    await poll({ syncForm: true });
  } catch (e) {
    alert(`GPIO: ${e.message}`);
  }
});

async function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function cooldownTapButton(ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const leftSec = Math.max(1, Math.ceil((end - Date.now()) / 1000));
    btnGpioTap.textContent = `${leftSec} с…`;
    await sleep(250);
  }
}

btnGpioTap.addEventListener('click', async () => {
  if (btnGpioTap.disabled) return;

  btnGpioTap.disabled = true;
  gpioSyncLocked = true;
  gpioState.disabled = true;
  const tapLabel = btnGpioTap.textContent;

  try {
    await sendCmd({
      cmd: 'gpio_tap',
      pin: Number(gpioPin.value),
      pulse_ms: GPIO_TAP_PULSE_MS,
    });
    setGpioUiState(true, { pulsing: true });
    btnGpioTap.textContent = 'Тап…';

    await sleep(GPIO_TAP_PULSE_MS);
    setGpioUiState(false);
    btnGpioTap.textContent = 'OFF…';

    await cooldownTapButton(GPIO_TAP_COOLDOWN_MS);
  } catch (e) {
    alert(`GPIO tap: ${e.message}`);
  } finally {
    gpioState.disabled = false;
    gpioSyncLocked = false;
    btnGpioTap.textContent = tapLabel;
    renderLinkBar();
    await poll({ syncForm: true });
  }
});

btnFlowReset.addEventListener('click', async () => {
  try {
    await sendCmd({ cmd: 'flow_reset' });
    await poll({ syncForm: false });
  } catch (e) {
    alert(`Поток: ${e.message}`);
  }
});

btnServoApply.addEventListener('click', async () => {
  try {
    if (servoAuto.checked) {
      await sendCmd({
        cmd: 'servo_auto',
        pin: Number(servoPin.value),
        enabled: true,
        pause_ms: 5000,
      });
    } else {
      await sendCmd({
        cmd: 'servo_set',
        pin: Number(servoPin.value),
        angle: Number(servoAngle.value),
      });
    }
    await poll({ syncForm: true });
  } catch (e) {
    alert(`Servo: ${e.message}`);
  }
});

window.addEventListener('offline', renderLinkBar);
window.addEventListener('online', () => {
  if (pollingActive) {
    poll({ syncForm: false });
  }
});

btnHrvMeasure?.addEventListener('click', async () => {
  if (!baseUrl) {
    alert('Сначала подключитесь к ESP32');
    return;
  }
  if (btnHrvMeasure.disabled) return;
  try {
    await loadHrvModule();
    window.OxyHrv?.open();
  } catch (e) {
    alert(`HRV: ${e.message}`);
  }
});

document.querySelectorAll('.window-pill').forEach((btn) => {
  if (!btn.dataset.window) return;
  btn.addEventListener('click', () => {
    setChartWindow(Number(btn.dataset.window) || 300);
  });
});

async function postTrendSources(next) {
  const res = await fetchWithTimeout(apiUrl('/api/trends/sources'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(next),
  }, 8000);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

document.querySelectorAll('.btn-trend').forEach((btn) => {
  btn.addEventListener('click', async () => {
    if (!btn.dataset.src) return;
    if (!baseUrl) {
      alert('Сначала подключитесь к ESP32');
      return;
    }
    const key = btn.dataset.src;
    const next = { ...trendSources, [key]: !trendSources[key] };
    btn.disabled = true;
    try {
      await postTrendSources(next);
      trendSources = next;
      syncTrendButtons();
      if (!anyTrendSource()) {
        clearHistory();
      } else {
        await hydrateTrends();
      }
      updateTrendsHint();
      drawAllCharts();
    } catch (e) {
      alert(`Тренд: ${e.message}`);
    } finally {
      btn.disabled = false;
    }
  });
});

if (hrViewBeltInput) {
  hrViewBeltInput.addEventListener('change', () => {
    hrViewBelt = !!hrViewBeltInput.checked;
    saveHrViewFlag(HR_VIEW_BELT_KEY, hrViewBelt);
    drawAllCharts();
  });
}
if (hrViewWellueInput) {
  hrViewWellueInput.addEventListener('change', () => {
    hrViewWellue = !!hrViewWellueInput.checked;
    saveHrViewFlag(HR_VIEW_WELLUE_KEY, hrViewWellue);
    drawAllCharts();
  });
}

syncHrViewInputs();

window.addEventListener('resize', () => {
  drawAllCharts();
  window.OxyHrv?.onResize();
});

updateTrendsHint();
drawAllCharts();

renderLinkBar();

if (location.hostname && location.hostname !== '') {
  hostInput.value = location.hostname;
  baseUrl = location.origin;
  startPolling();
}
